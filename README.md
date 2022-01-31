<center><img src="https://i.imgur.com/nMJp1oA.png"/></center>

# PSKP (Process-Context Specific Kernel Patches)

This project allows you to patch your kernel only in a specific process. It is highly experimental and will most likely cause your system to crash. Please install some form of virtualization before messing around
with this project/library! This software is provided as-is, I have no plans on updating this code (except to add 2mb page support)... 

If you are interested in how this code works you can read about it here: [https://back.engineering/post/nasa-patch/](https://back.engineering/post/nasa-patch/).

# example

```cpp
	nasa::mem_ctx my_proc(kernel, GetCurrentProcessId());
	nasa::patch_ctx kernel_patch(&my_proc);

	const auto function_addr =
		reinterpret_cast<void*>(
			util::get_module_export("win32kbase.sys", "NtDCompositionRetireFrame"));

	const auto new_patch_page = kernel_patch.patch(function_addr);
	std::cout << "[+] new_patch_page: " << new_patch_page << std::endl;
	*(short*)new_patch_page = 0xDEAD; // this patch will only be viewable inside of your context...
```

# credit
* buck#0001 - inspiration for most of this