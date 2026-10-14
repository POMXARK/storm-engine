# Local FMOD recipe for Linux: the piratesahoy remote only has Windows binaries.
# The SDK (FMOD Engine 2.02.x, Linux) is downloaded manually from fmod.com; its path is passed via FMOD_SDK (see install.sh).
import os
from os.path import join
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy


class FmodConan(ConanFile):
    name = "fmod"
    version = "2.02.05+1"
    user = "piratesahoy+storm-engine"
    channel = "stable"
    settings = "os", "build_type"

    def validate(self):
        if self.settings.os != "Linux":
            raise ConanInvalidConfiguration("Use the piratesahoy remote package on Windows")

    def package(self):
        sdk = os.environ.get("FMOD_SDK")
        if not sdk or not os.path.isdir(join(sdk, "api", "core")):
            raise ConanInvalidConfiguration("Set FMOD_SDK to the unpacked fmodstudioapi*linux folder")
        core = join(sdk, "api", "core")
        copy(self, "*.h*", dst=join(self.package_folder, "include"), src=join(core, "inc"))
        copy(self, "libfmod*.so*", dst=join(self.package_folder, "lib"), src=join(core, "lib", "x86_64"))

    def package_info(self):
        self.cpp_info.libs = ["fmodL" if self.settings.build_type == "Debug" else "fmod"]
