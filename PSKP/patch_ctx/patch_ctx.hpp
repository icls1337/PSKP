#pragma once
#include "../mem_ctx/mem_ctx.hpp"

namespace nasa
{
	class patch_ctx
	{
	public:
		explicit patch_ctx(mem_ctx* mem_ctx);
		void* patch(void* kernel_addr);
		__forceinline void enable()
		{
			mapped_pml4e->pfn = new_pml4e.pfn;
		}

		__forceinline void disable()
		{
			mapped_pml4e->pfn = old_pml4e.pfn;
		}

	private:
		auto make_pdpt(const pt_entries& kernel_entries, void* kernel_addr)->std::pair<void*, void*>;
		auto make_pd(const pt_entries& kernel_entries, void* kernel_addr)->std::pair<void*, void*>;
		auto make_pt(const pt_entries& kernel_entries, void* kernel_addr)->std::pair<void*, void*>;
		auto make_page(const pt_entries& kernel_entries, void* kernel_addr)->std::pair<void*, void*>;

		mem_ctx* mem_ctx;
		pt_entries new_entries;
		pml4e new_pml4e;
		pml4e old_pml4e;
		void* kernel_addr;
		ppml4e mapped_pml4e;
	};
}