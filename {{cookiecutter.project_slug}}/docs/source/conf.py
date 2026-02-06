# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information
import os

# get the version from the VERSION file (the one without extension) in the root directory
# read the VERSION file
version_file_path = os.path.join(os.path.dirname(__file__), '..', '..', 'VERSION')
if os.path.exists(version_file_path):
    with open(version_file_path, 'r') as version_file:
        release = version_file.read().strip()

project = '{{ cookiecutter.project_slug }}'
copyright = '2026, {{ cookiecutter.author_name }}'
author = '{{ cookiecutter.author_name }}'
release = '1.0.0'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = []

templates_path = ['_templates']
html_static_path = ['_static']

# html_logo = '_static/logo.png'

exclude_patterns = []



# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'furo'
html_static_path = ['_static']
