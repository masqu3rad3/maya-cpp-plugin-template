import os
import sys
import json
from pathlib import Path

PROJECT_DIRECTORY = os.path.realpath(os.path.curdir)
sys.path.insert(0, PROJECT_DIRECTORY)  # add the working directory to the path
from package import inject_utils

# This template will contain all available definitions for the project.

DEFINITIONS_TEMPLATE = {
  "windows_devkits":{
    "2022": "https://autodesk-adn-transfer.s3-us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2022/Autodesk_Maya_2022_5_Update_DEVKIT_Windows.zip",
    "2023": "https://autodesk-adn-transfer.s3-us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2023/Autodesk_Maya_2023_3_Update_DEVKIT_Windows.zip",
    "2024": "https://autodesk-adn-transfer.s3-us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2024/Autodesk_Maya_2024_2_Update_DEVKIT_Windows.zip",
    "2025": "https://autodesk-adn-transfer.s3.us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2025/Autodesk_Maya_2025_3_Update_DEVKIT_Windows.zip",
    "2026": "https://autodesk-adn-transfer.s3.us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2026/Autodesk_Maya_2026_1_Update_DEVKIT_Windows.zip"
  },
  "linux_devkits":{
    "2022": "https://autodesk-adn-transfer.s3-us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2022/Autodesk_Maya_2022_5_Update_DEVKIT_Linux.tgz",
    "2023": "https://autodesk-adn-transfer.s3-us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2023/Autodesk_Maya_2023_3_Update_DEVKIT_Linux.tgz",
    "2024": "https://autodesk-adn-transfer.s3-us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2024/Autodesk_Maya_2024_2_Update_DEVKIT_Linux.tgz",
    "2025": "https://autodesk-adn-transfer.s3.us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2025/Autodesk_Maya_2025_3_Update_DEVKIT_Linux.tgz",
    "2026": "https://autodesk-adn-transfer.s3.us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2026/Autodesk_Maya_2026_1_Update_DEVKIT_Linux.tgz"
  },
  "darwin_devkits":{
      "2022": "https://autodesk-adn-transfer.s3-us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2022/Autodesk_Maya_2022_5_Update_DEVKIT_Mac.dmg",
      "2023": "https://autodesk-adn-transfer.s3-us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2023/Autodesk_Maya_2023_3_Update_DEVKIT_Mac.dmg",
      "2024": "https://autodesk-adn-transfer.s3-us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2024/Autodesk_Maya_2024_2_Update_DEVKIT_Mac.dmg",
      "2025": "https://autodesk-adn-transfer.s3.us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2025/Autodesk_Maya_2025_3_Update_DEVKIT_Mac.dmg",
      "2026": "https://autodesk-adn-transfer.s3.us-west-2.amazonaws.com/ADN+Extranet/M%26E/Maya/devkit+2026/Autodesk_Maya_2026_3_Update_DEVKIT_Mac.dmg"
  },
  "local_devkits_relative_path": "{{ cookiecutter.devkit_directory }}",
  "target_maya_versions": [
    "2022",
    "2023",
    "2024",
    "2025",
    "2026"
  ],
  "project_slug": "{{ cookiecutter.project_slug }}",
  "project_name": "{{ cookiecutter.project_name }}"
}

PROJECT_PATH = Path.cwd()
BLUEPRINT_PATH = PROJECT_PATH / '_blueprint' / 'plugin_template'
SRC_PATH = PROJECT_PATH / 'src'

# Get the list of plugins from cookiecutter
plugin_list_raw = "{{ cookiecutter.plugin_names }}"
plugins = [p.strip() for p in plugin_list_raw.split(',') if p.strip()]

for plugin in plugins:
    main_cmake_file = PROJECT_PATH / 'CMakeLists.txt'
    inject_utils.add_plugin(plugin, main_cmake_file, BLUEPRINT_PATH, SRC_PATH)

# Ge the list of maya versions from cookiecutter
maya_versions_raw = "{{ cookiecutter.maya_versions }}"
maya_versions = [v.strip() for v in maya_versions_raw.split(',') if v.strip()]

# initial_definitions = deepcopy(DEFINITIONS_TEMPLATE)
initial_definitions = {}
initial_definitions["project_slug"] = DEFINITIONS_TEMPLATE["project_slug"]
initial_definitions["project_name"] = DEFINITIONS_TEMPLATE["project_name"]
initial_definitions["local_devkits_relative_path"] = DEFINITIONS_TEMPLATE["local_devkits_relative_path"]
initial_definitions["target_maya_versions"] = []
initial_definitions["windows_devkits"] = {}
initial_definitions["linux_devkits"] = {}
initial_definitions["darwin_devkits"] = {}
for version in maya_versions:
    if version in DEFINITIONS_TEMPLATE["target_maya_versions"]:
        initial_definitions["target_maya_versions"].append(version)
        initial_definitions["windows_devkits"][version] = DEFINITIONS_TEMPLATE["windows_devkits"][version]
        initial_definitions["linux_devkits"][version] = DEFINITIONS_TEMPLATE["linux_devkits"][version]
        initial_definitions["darwin_devkits"][version] = DEFINITIONS_TEMPLATE["darwin_devkits"][version]
    else:
        sys.stdout.write(f"Warning: Maya version {version} is not in the template definitions and will be skipped.")

# Save the definitions to a JSON file in the project directory
definitions_path = PROJECT_PATH / "package" / "definitions.json"
with open(definitions_path, "w", encoding="utf-8") as f:
    json.dump(initial_definitions, f, indent=4)

release_ci_path = PROJECT_PATH / ".github" / "workflows" / "release.yml"
inject_utils.inject_release_ci(release_ci_path, initial_definitions)