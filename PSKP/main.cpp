#include <iostream>
#include "vdm_ctx/vdm_ctx.h"
#include "mem_ctx/mem_ctx.hpp"
#include "patch_ctx/patch_ctx.hpp"

int __cdecl main(int argc, char** argv)
{
	const auto [drv_handle, drv_key] = vdm::load_drv();
	if (drv_handle == INVALID_HANDLE_VALUE)
	{
		std::printf("[!] unable to load vdm driver...\n");
		std::cin.get();
		return -1;
	}

	vdm::vdm_ctx v_ctx;
	nasa::mem_ctx my_proc(v_ctx);
	nasa::patch_ctx kernel_patch(&my_proc);

	const auto function_addr =
		reinterpret_cast<std::uintptr_t>(
			util::get_kmodule_export(
				"win32kbase.sys",
				"NtDCompositionRetireFrame"
			));

	const auto new_patch_page = kernel_patch.patch((void*)function_addr);
	std::cout << "[+] new_patch_page: " << new_patch_page << std::endl;
	*reinterpret_cast<short*>(new_patch_page) = 0xDEAD;

	std::cout << "[+] kernel MZ (before patch): " << std::hex << v_ctx.rkm<short>(function_addr) << std::endl;
	kernel_patch.enable();
	std::cout << "[+] kernel MZ (patch enabled): " << std::hex << v_ctx.rkm<short>(function_addr) << std::endl;
	kernel_patch.disable();
	std::cout << "[+] kernel MZ (patch disabled): " << std::hex << v_ctx.rkm<short>(function_addr) << std::endl;

	if (!vdm::unload_drv(drv_handle, drv_key))
	{
		std::printf("[!] unable to unload driver...\n");
		std::cin.get();
		return -1;
	}
	std::cin.get();
}
