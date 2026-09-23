import os
import kconfiglib

script_dir = os.path.dirname(os.path.abspath(__file__))
kconf = kconfiglib.Kconfig(os.path.join(script_dir, "Kconfig"))
kconf.load_config(os.path.join(script_dir, ".config"))

with open(os.path.join(script_dir, "config.cmake"), "w") as f:
    for sym in kconf.unique_defined_syms:
        if sym.name == "BUILD_PROJECT_SDK_PATH":
            abs_path = os.path.abspath(os.path.join(script_dir, sym.str_value))
            f.write(f"set({sym.name} {abs_path})\n")
        else:
            f.write(f"set({sym.name} {sym.str_value})\n")
