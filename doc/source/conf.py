project = "actorpp"
copyright = "2023, Thomas Nixon"
author = "Thomas Nixon"

extensions = []

extensions.append("breathe")
breathe_projects = {"actorpp": "./_doxygen/xml"}
breathe_default_project = "actorpp"

extensions.append("exhale")
from textwrap import dedent

exhale_args = dict(
    containmentFolder="./api",
    rootFileName="library_root.rst",
    rootFileTitle="API",
    doxygenStripFromPath="../../include",
    createTreeView=True,
    exhaleExecutesDoxygen=True,
    exhaleDoxygenStdin=dedent(
        """
            INPUT = ../../include
        """
    ),
    afterTitleDescription=dedent(
        """
        The main components of interest are :class:`actorpp::Actor` for
        creating actors, :class:`actorpp::ActorThread` for running actors in
        threads, and :class:`actorpp::Channel` for communicating between/with
        actors.
        """
    ),
)

extensions.append("sphinx.ext.intersphinx")
intersphinx_mapping = {
    "eshet": ("https://eshet.readthedocs.io/en/latest/", None),
    "eshetcpp": ("https://eshet.readthedocs.io/projects/eshetcpp/en/latest/", None),
}

extensions.append("myst_parser")

templates_path = ["_templates"]
exclude_patterns = []

html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]

primary_domain = "cpp"
highlight_language = "cpp"
