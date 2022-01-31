#include "patch_ctx.hpp"

namespace nasa
{
	patch_ctx::patch_ctx(nasa::mem_ctx* mem_ctx)
		:
		mem_ctx(mem_ctx)
	{}

	void* patch_ctx::patch(void* kernel_addr)
	{
		virt_addr_t kernel_addr_t{ kernel_addr };
		pt_entries kernel_entries;

		if (!mem_ctx->virt_to_phys(kernel_entries, kernel_addr))
			return {};
		
		const auto [new_pdpt_phys, new_pdpt_virt] = make_pdpt(kernel_entries, kernel_addr);
		const auto [new_pd_phys, new_pd_virt] = make_pd(kernel_entries, kernel_addr);
		const auto [new_pt_phys, new_pt_virt] = make_pt(kernel_entries, kernel_addr);
		const auto [new_page_phys, new_page_virt] = make_page(kernel_entries, kernel_addr);

		// link pdpte to pd
		(reinterpret_cast<ppdpte>(new_pdpt_virt) + kernel_addr_t.pdpt_index)->pfn = reinterpret_cast<std::uintptr_t>(new_pd_phys) >> 12;
		if (kernel_entries.pd.second.large_page)
		{
			// link pde to new page if its 2mb
			(reinterpret_cast<ppde>(new_pd_virt) + kernel_addr_t.pd_index)->pfn = reinterpret_cast<std::uintptr_t>(new_page_phys) >> 12;
		}
		else
		{
			// link pde to pt
			(reinterpret_cast<ppde>(new_pd_virt) + kernel_addr_t.pd_index)->pfn = reinterpret_cast<std::uintptr_t>(new_pt_phys) >> 12;
			// link pte to page (1kb)
			(reinterpret_cast<ppte>(new_pt_virt) + kernel_addr_t.pt_index)->pfn = reinterpret_cast<std::uintptr_t>(new_page_phys) >> 12;
		}

		mapped_pml4e = 
			reinterpret_cast<ppml4e>(
				mem_ctx->set_page(
					kernel_entries.pml4.first));

		new_pml4e = kernel_entries.pml4.second;
		new_pml4e.pfn = reinterpret_cast<std::uintptr_t>(new_pdpt_phys) >> 12;
		old_pml4e = kernel_entries.pml4.second;
		return reinterpret_cast<void*>((std::uintptr_t)new_page_virt + kernel_addr_t.offset);
	}

	auto patch_ctx::make_pdpt(const pt_entries& kernel_entries, void* kernel_addr) -> std::pair<void*, void*>
	{
		virt_addr_t kernel_addr_t{ kernel_addr };
		const auto pdpt = reinterpret_cast<ppdpte>(
			mem_ctx->set_page(reinterpret_cast<void*>(
				kernel_entries.pml4.second.pfn << 12)));

		const auto new_pdpt =
			reinterpret_cast<ppdpte>(
				VirtualAlloc(
					NULL,
					PAGE_4KB,
					MEM_COMMIT | MEM_RESERVE,
					PAGE_READWRITE
				));

		PAGE_IN(new_pdpt, PAGE_4KB);
		// copy over pdpte's
		for (auto idx = 0u; idx < 512; ++idx)
			new_pdpt[idx] = pdpt[idx];

		// get physical address of new pdpt
		pt_entries new_pdpt_entries;
		const auto physical_addr =
			mem_ctx->virt_to_phys(new_pdpt_entries, new_pdpt);

		return { physical_addr, new_pdpt };
	}

	auto patch_ctx::make_pd(const pt_entries& kernel_entries, void* kernel_addr) -> std::pair<void*, void*>
	{
		virt_addr_t kernel_addr_t{ kernel_addr };
		const auto new_pd = 
			reinterpret_cast<ppde>(
				VirtualAlloc(
					NULL,
					PAGE_4KB,
					MEM_COMMIT | MEM_RESERVE, 
					PAGE_READWRITE
				));

		PAGE_IN(new_pd, PAGE_4KB);
		const auto pd = reinterpret_cast<ppde>(
			mem_ctx->set_page(
				reinterpret_cast<void*>(
					kernel_entries.pdpt.second.pfn << 12)));

		// copy over pde's
		for (auto idx = 0u; idx < 512; ++idx)
			new_pd[idx] = pd[idx];

		//
		// get physical address of new pd
		//
		pt_entries pd_entries;
		const auto physical_addr =
			mem_ctx->virt_to_phys(pd_entries, new_pd);

		return { physical_addr, new_pd };
	}

	auto patch_ctx::make_pt(const pt_entries& kernel_entries, void* kernel_addr) -> std::pair<void*, void*>
	{
		// if this address is a 2mb mapping.
		if (!kernel_addr || kernel_entries.pd.second.large_page) 
			return {};

		virt_addr_t kernel_addr_t{ kernel_addr };
		const auto new_pt = 
			reinterpret_cast<ppte>(
				VirtualAlloc(
					NULL,
					PAGE_4KB,
					MEM_COMMIT | MEM_RESERVE,
					PAGE_READWRITE
				));

		PAGE_IN(new_pt, PAGE_4KB);
		const auto pt = reinterpret_cast<ppte>(
			mem_ctx->set_page(
				reinterpret_cast<void*>(
					kernel_entries.pd.second.pfn << 12)));

		// copy over pte's
		for (auto idx = 0u; idx < 512; ++idx)
			new_pt[idx] = pt[idx];

		// get physical address of new pt
		pt_entries entries;
		const auto physical_addr =
			mem_ctx->virt_to_phys(entries, new_pt);

		return { physical_addr, new_pt };
	}

	auto patch_ctx::make_page(const pt_entries& kernel_entries, void* kernel_addr) -> std::pair<void*, void*>
	{
		virt_addr_t kernel_addr_t{ kernel_addr };

		// if its a 2mb page
		if (kernel_entries.pd.second.large_page)
		{
			const auto new_page = 
				VirtualAlloc(
					NULL,
					PAGE_4KB * 512 * 2, // 4mb
					MEM_COMMIT | MEM_RESERVE,
					PAGE_READWRITE
				);

			PAGE_IN(new_page, PAGE_4KB * 512);
			// copy 2mb one page at a time.
			for (auto idx = 0u; idx < 512; ++idx)
			{
				const auto old_page = mem_ctx->set_page(reinterpret_cast<void*>((kernel_entries.pt.second.pfn << 12) + (idx * 0x1000)));
				memcpy((void*)((std::uintptr_t)new_page + (idx * 0x1000)), old_page, 0x1000);
			}

			pt_entries new_page_entries;
			const auto new_page_phys = mem_ctx->virt_to_phys(new_page_entries, new_page);
			return { new_page_phys, new_page };
		}
		else // 1kb
		{
			const auto new_page = 
				VirtualAlloc(
					NULL,
					PAGE_4KB, 
					MEM_COMMIT | MEM_RESERVE,
					PAGE_READWRITE
				);

			PAGE_IN(new_page, PAGE_4KB);
			const auto old_page =
				mem_ctx->set_page(
					reinterpret_cast<void*>(
						kernel_entries.pt.second.pfn << 12));

			memcpy(new_page, old_page, PAGE_4KB);
			pt_entries new_page_entries;
			const auto new_page_phys = 
				mem_ctx->virt_to_phys(
					new_page_entries, new_page);

			return { new_page_phys, new_page };
		}
	}
}