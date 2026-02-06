"""Drag & Drop installer for Maya 2022+"""
from pathlib import Path
import sys

# confirm the maya python interpreter
CONFIRMED = False
try:
    from maya import cmds

    CONFIRMED = True
except ImportError:
    CONFIRMED = False


def onMayaDroppedPythonFile(*args, **kwargs):
    # check the pyhon interpreter version
    if sys.version_info.major < 3:
        cmds.confirmDialog(title='ERROR:', message="{{ cookiecutter.project_slug }} requires Python version 3.6 and higher. Current Maya Python interpreter is not compatible. \n\nAborting.", button=['OK'],
                       defaultButton='OK')
        return
    _add_module()


def _add_module():
    # module directory is the project root which is one level above
    _module_dir = Path(__file__).parent.parent

    module_file_content = f"""+ {{ cookiecutter.project_slug }} 1.0.0 {_module_dir.as_posix()}
MAYA_PLUG_IN_PATH +:= build/Debug
MAYA_PLUG_IN_PATH +:= src
PYTHONPATH +:= scripts
"""

    user_module_dir = Path(cmds.internalVar(uad=True), "modules")
    # create the modules directory if it does not exist
    user_module_dir.mkdir(parents=True, exist_ok=True)
    user_module_file = user_module_dir / "{{ cookiecutter.project_slug }}_dev.mod"
    # remove existing module file if it exists
    if user_module_file.is_file():
        user_module_file.unlink()

    _f = open(user_module_file, "w+")
    _f.writelines(module_file_content)
    _f.close()

    # first time initialize
    cmds.confirmDialog(
        title="{{ cookiecutter.project_name }} Dev",
        message="{{ cookiecutter.project_name }} Dev Module Installed. Please restart Maya."
    )

