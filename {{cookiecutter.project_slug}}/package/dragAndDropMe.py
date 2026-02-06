"""Drag & Drop installer for Maya 2022+"""
from pathlib import Path
import platform
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
        cmds.confirmDialog(title='ERROR:', message="{{ cookiecutter.project_slug }} requires Python version 3.6 and higher. Current Maya Python interpreter is not compatible. \n\nAborting.", button=['OK'],
                       defaultButton='OK')
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
        title="{{ cookiecutter.project_slug }}",
        message="{{ cookiecutter.project_slug }} installed. Please restart Maya to see the shelf and menu items."
    )
