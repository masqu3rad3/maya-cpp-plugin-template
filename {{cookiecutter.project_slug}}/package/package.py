"""Package management."""

import platform
import argparse
import sys
import os
import logging
from pathlib import Path
import json
import shutil
import subprocess

import inject_utils

LOG = logging.getLogger(__name__)

PACKAGE_ROOT = Path(__file__).parent
REPO_ROOT = PACKAGE_ROOT.parent
ROOT_CMAKELISTS = REPO_ROOT / "CMakeLists.txt"

DEFINITIONS_FILE = PACKAGE_ROOT / "definitions.json"
DEFINITIONS = json.load(open(DEFINITIONS_FILE))

BLUEPRINT_PATH = REPO_ROOT / "_blueprint" / "plugin_template"

VERSION_FILE = REPO_ROOT / "VERSION"
VERSION = ""

with open(VERSION_FILE.as_posix(), 'r') as version_file:
    VERSION = version_file.read().strip()

OS = platform.system().lower()

def add_plugin_to_cmakelists(plugin_name: str):
    """Add a subdirectory to the root CMakeLists.txt."""
    inject_utils.add_plugin(plugin_name, ROOT_CMAKELISTS, BLUEPRINT_PATH, REPO_ROOT / "src")

    # if not ROOT_CMAKELISTS.exists():
    #     raise FileNotFoundError(f"CMakeLists.txt not found at {ROOT_CMAKELISTS}")
    #
    # with open(ROOT_CMAKELISTS, "r") as file:
    #     lines = file.readlines()
    #
    # # Check if the subdirectory is already included
    # include_line = f"add_subdirectory(src/{plugin_name})\n"
    # if include_line in lines:
    #     sys.stdout.write(f"{plugin_name} is already included in CMakeLists.txt.\n")
    # else:
    #     # Add the subdirectory at the end of the file
    #     with open(ROOT_CMAKELISTS, "a") as file:
    #         file.write(f"\n{include_line}")
    #     sys.stdout.write(f"Added {plugin_name} to CMakeLists.txt.\n")
    #
    # # check if the plugin folder is in the src directory
    # plugin_folder = REPO_ROOT / "src" / plugin_name
    # if plugin_folder.exists():
    #     sys.stdout.write(f"{plugin_folder} already exists.\n")
    # else:
    #     plugin_folder.mkdir(parents=True, exist_ok=True)
    #     sys.stdout.write(f"Plugin folder {plugin_folder} created.\n")

def _download_devkit_linux(download_link, devkit_path):
    """Download the devkit for Linux."""
    try:
        subprocess.check_call(["curl", "-L", download_link, "-o", f"{devkit_path / 'devkitBase.tar.gz'}"])
        # Extract the tar.gz file
        sys.stdout.write("Extracting the file...\n")
        subprocess.check_call(["tar", "-xzf", f"{devkit_path / 'devkitBase.tar.gz'}", "-C", devkit_path])
        # Remove the tar.gz file after extraction
        (devkit_path / "devkitBase.tar.gz").unlink()
        sys.stdout.write(f"Devkit downloaded and extracted successfully.\n")
    except subprocess.CalledProcessError as e:
        sys.stdout.write(f"Failed to download or extract devkit. Error: {e}\n")

def _download_devkit_mac(download_link, devkit_path):
    """Download the devkit for Mac."""
    try:
        subprocess.check_call(["curl", "-L", download_link, "-o", f"{devkit_path / 'devkitBase.zip'}"])
        # Unzip the downloaded file
        sys.stdout.write("Extracting the file...\n")
        subprocess.check_call(["unzip", f"{(devkit_path / 'devkitBase.zip').resolve()}", "-d", devkit_path])
        # Remove the zip file after extraction
        (devkit_path / "devkitBase.zip").unlink()
        sys.stdout.write(f"Devkit downloaded and extracted successfully.\n")
    except subprocess.CalledProcessError as e:
        sys.stdout.write(f"Failed to download or extract devkit. Error: {e}\n")

def _download_devkit_win(download_link, devkit_path):
    """Download the devkit for Windows."""
    try:
        subprocess.check_call(["curl", "-L", download_link, "-o", f"{devkit_path / 'devkitBase.zip'}"])
        # Unzip the downloaded file using PowerShell
        sys.stdout.write("Extracting the file...\n")
        subprocess.check_call([
            "powershell", "-Command",
            f"Expand-Archive -LiteralPath '{(devkit_path / 'devkitBase.zip').resolve()}' -DestinationPath '{devkit_path.resolve()}' -Force"
        ])
        # Remove the zip file after extraction
        (devkit_path / "devkitBase.zip").unlink()
        sys.stdout.write(f"Devkit downloaded and extracted successfully.\n")
    except subprocess.CalledProcessError as e:
        sys.stdout.write(f"Failed to download or extract devkit. Error: {e}\n")

def validate_local_devkits(maya_version=None):
    """Validate the local devkits."""
    target_maya_versions = [maya_version] if maya_version else DEFINITIONS["target_maya_versions"]
    local_devkits = (REPO_ROOT / DEFINITIONS["local_devkits_relative_path"])
    # check if the local devkits folder exists
    local_devkits.mkdir(parents=True, exist_ok=True)
    for version in target_maya_versions:
        devkit_path = local_devkits / version
        if not (devkit_path / "devkitBase").exists():
            sys.stdout.write(f"Devkit for Maya {version} not found at {devkit_path}. Attempting to download from the definitions.\n")
            devkit_path.mkdir(parents=True, exist_ok=True)
            download_link = DEFINITIONS[f"{OS}_devkits"].get(version)
            if download_link:
                sys.stdout.write(f"Downloading devkit for Maya {version} from {download_link}...\n")
                if OS == "linux":
                    _download_devkit_linux(download_link, devkit_path)
                elif OS == "darwin":
                    _download_devkit_mac(download_link, devkit_path)
                elif OS == "windows":
                    _download_devkit_win(download_link, devkit_path)
        else:
            sys.stdout.write(f"Devkit for Maya {version} found at {devkit_path.resolve()}.\n")

def build_plugins(maya_version, build_type="Debug", continue_on_error=False):
    """Build the plugins using CMake."""
    build_dir = REPO_ROOT / "build"
    # return build_dir
    # delete the build directory if it exists
    if build_dir.exists():
        shutil.rmtree(build_dir.as_posix())
    try:
        subprocess.check_call(["cmake", "-S", str(REPO_ROOT), "-B", str(build_dir), f"-DCMAKE_BUILD_TYPE={build_type}", f"-DMAYA_VERSION={maya_version}"])
        subprocess.check_call(["cmake", "--build", str(build_dir), "--config", build_type])
        sys.stdout.write("Plugins built successfully.\n")
        return build_dir
    except subprocess.CalledProcessError as e:
        if continue_on_error:
            sys.stdout.write(f"Failed to build plugins. Error: {e}\n")
        else:
            raise RuntimeError(f"Failed to build plugins. Error: {e}") from e

def dev_deploy(version=None):
    """Deploy the plugin(s) for a specific Maya version. Or if version is None, deploy for all target versions."""
    validate_local_devkits()
    extensions = {
        "windows": ".mll",
        "linux": ".so",
        "darwin": ".bundle"
    }
    deploy_root_path = REPO_ROOT / "_dev_deploy"
    # modules_path = deploy_root_path / "modules"
    # deploy_path = modules_path / DEFINITIONS["project_slug"]
    plugins_path = deploy_root_path / "plugins"
    plugins_path.mkdir(parents=True, exist_ok=True)
    deploy_versions = [version] if version else DEFINITIONS["target_maya_versions"]
    for maya_version in deploy_versions:
        build_dir = build_plugins(maya_version, build_type="Release")
        plugin_path = plugins_path / f"{OS}-{maya_version}"
        plugin_path.mkdir(exist_ok=True)
        # collect the built plugins and copy to the deploy folder
        collected_plugins = build_dir.rglob(f"*{extensions[OS]}")
        for item in collected_plugins:
            shutil.copy(item, plugin_path / item.name)
            sys.stdout.write(f"Copied {item.name} to deploy folder.\n")

    # Maya Modules injections
    user_maya_folder = Path(_get_home_dir()) / "Documents" / "maya"
    if not user_maya_folder.exists():
        raise ValueError("No Maya version can be found in the user's documents directory")
    modules_file_path = user_maya_folder / "modules" / f"{DEFINITIONS['project_slug']}_dev.mod"
    modules_file_path.parent.mkdir(parents=True, exist_ok=True)
    with open(modules_file_path, "w") as mod_file:
        mod_file.writelines(_generate_dev_mod())


def release(version=None):
    """Make a deployable package."""
    validate_local_devkits()
    extensions = {
        "windows": ".mll",
        "linux": ".so",
        "darwin": ".bundle"
    }
    deploy_root_path = REPO_ROOT / "release"
    modules_path = deploy_root_path / "modules"
    deploy_path = modules_path / DEFINITIONS["project_slug"]
    plugins_path = deploy_path / "plugins"
    plugins_path.mkdir(parents=True, exist_ok=True)
    deploy_versions = [version] if version else DEFINITIONS["target_maya_versions"]
    for maya_version in deploy_versions:
        build_dir = build_plugins(maya_version, build_type="Release")
        plugin_path = plugins_path / f"{OS}-{maya_version}"
        plugin_path.mkdir(exist_ok=True)
        # collect the built plugins and copy to the deploy folder
        collected_plugins = build_dir.rglob(f"*{extensions[OS]}")
        for item in collected_plugins:
            shutil.copy(item, plugin_path / item.name)
            sys.stdout.write(f"Copied {item.name} to deploy folder.\n")

    # if there is a scripts folder under the src, copy it under the deploy_path
    src_scripts_path = REPO_ROOT / "src" / "scripts"
    if src_scripts_path.exists():
        deploy_scripts_path = deploy_path / "scripts"
        if deploy_scripts_path.exists():
            shutil.rmtree(deploy_scripts_path.as_posix())
        shutil.copytree(src_scripts_path, deploy_scripts_path)
        sys.stdout.write(f"Copied scripts to deploy folder.\n")

    # create the .mod file
    mod_file_path = modules_path / f"{DEFINITIONS['project_slug']}.mod"
    with open(mod_file_path, "w") as mod_file:
        mod_file.writelines(_generate_release_mod())
    sys.stdout.write(f"Generated .mod file at {mod_file_path.resolve()}.\n")
    _save_drag_and_drop_me_script(deploy_root_path / "dragAndDropMe.py")

def _generate_release_mod():
    """Generate the content for the .mod file.

    We don't necessarily need to collect which plugins are built for the .mod file
    since we will add the plugins path to the .mod file,
    Maya will automatically load all plugins under that path.
    Even the folders are empty, it's still good to have them in the .mod file.
    """
    deploy_versions = DEFINITIONS["target_maya_versions"]
    for _platform, _scode in {"windows":"win64", "linux":"linux", "darwin":"mac"}.items():
        for maya_version in deploy_versions:
            yield f"+ MAYAVERSION:{maya_version} PLATFORM:{_scode} {DEFINITIONS['project_slug']} {VERSION} {DEFINITIONS['project_slug']}\n"
            yield f"MAYA_PLUG_IN_PATH +:= plugins\\{_platform}-{maya_version}\n"
            yield "\n"

def _generate_dev_mod():
    """Generate the content for the .mod file.

    THIS IS FOR DEVELOPMENT PURPOSE ONLY, NOT FOR RELEASE.
    We don't necessarily need to collect which plugins are built for the .mod file
    since we will add the plugins path to the .mod file,
    Maya will automatically load all plugins under that path.
    Even the folders are empty, it's still good to have them in the .mod file.
    """
    deploy_versions = DEFINITIONS["target_maya_versions"]
    for _platform, _scode in {"windows":"win64", "linux":"linux", "darwin":"mac"}.items():
        for maya_version in deploy_versions:
            yield f"+ MAYAVERSION:{maya_version} PLATFORM:{_scode} {DEFINITIONS['project_slug']} {VERSION} {REPO_ROOT.as_posix()}\n"
            yield f"MAYA_PLUG_IN_PATH +:= _dev_deploy/plugins/{_platform}-{maya_version}\n"
            yield f"PYTHONPATH +:= src/scripts\n"
            yield "\n"

def _save_drag_and_drop_me_script(path_to_save):
    """Generate the drag and drop script for easy installation."""
    content = f"""
from pathlib import Path
import sys
import shutil

# confirm the maya python interpreter
CONFIRMED = False
try:
    from maya import cmds

    CONFIRMED = True
except ImportError:
    CONFIRMED = False


def onMayaDroppedPythonFile(*args, **kwargs):
    if sys.version_info.major < 3:
        cmds.confirmDialog(title='ERROR:', message="{DEFINITIONS['project_name']} requires Python version 3.6 and higher. Current Maya Python interpreter is not compatible. \\n\\nAborting.", button=['OK'], defaultButton='OK')
        return

    _add_module()


def _add_module():
    # Define source and destination paths
    source_modules = Path(__file__).parent / "modules"
    user_maya_dir = Path(cmds.internalVar(uad=True))
    destination_modules = user_maya_dir / "modules"

    # Ensure the destination 'modules' folder exists
    destination_modules.mkdir(parents=True, exist_ok=True)

    # Copy contents of the source 'modules' folder to the destination
    for item in source_modules.iterdir():
        destination_item = destination_modules / item.name
        if item.is_dir():
            # Merge directory contents
            for sub_item in item.rglob("*"):
                relative_path = sub_item.relative_to(item)
                target_path = destination_item / relative_path
                if sub_item.is_dir():
                    target_path.mkdir(parents=True, exist_ok=True)
                else:
                    shutil.copy2(sub_item, target_path)
        else:
            # Copy file (overwrite if exists)
            shutil.copy2(item, destination_item)

    # Confirm installation
    cmds.confirmDialog(
        title="{DEFINITIONS['project_name']}",
        message="{DEFINITIONS['project_name']} installed. Please restart Maya to see the shelf and menu items."
    )

"""
    # save the content to the specified path
    with open(path_to_save, "w") as f:
        f.write(content)
    sys.stdout.write(f"Generated drag and drop installer script at {path_to_save.resolve()}).\n")

def _get_home_dir():
    """Get the user home directory."""
    # expanduser does not always return the same result (in Maya it returns user/Documents).
    # This returns the true user folder for all platforms and dccs"""
    if OS == "windows":
        return os.path.normpath(os.getenv("USERPROFILE"))
    return os.path.normpath(os.getenv("HOME"))

if __name__ == "__main__":
    # example usage: python package.py --add-plugin my_plugin

    parser = argparse.ArgumentParser(description="Package management script.")
    parser.add_argument("--add-plugin", type=str, help="Add a plugin to CMakeLists.txt")
    parser.add_argument("--validate-local-devkits", action="store_true", help="Validate local devkits. If no version is specified, it will attempt to download from the definitions.json.")
    parser.add_argument("--build", type=str, help="Build the plugin for given Maya version.")
    parser.add_argument("--dev", nargs='?', const=None, type=str,
                        default=argparse.SUPPRESS,
                        help="Build and test deploy the plugin for given Maya version. If no value is provided (just `--dev`), it will be parsed as None; if a version is provided, it will be parsed as that string.")
    parser.add_argument("--release", action="store_true", help="Prepare the release package.")

    args = parser.parse_args()

    if args.add_plugin:
        add_plugin_to_cmakelists(args.add_plugin)

    if args.validate_local_devkits:
        validate_local_devkits()

    if args.build:
        build_plugins(args.build)

    if args.release:
        release()

    if hasattr(args, "dev"):
        dev_deploy(args.dev)

