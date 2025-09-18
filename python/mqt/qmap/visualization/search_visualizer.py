# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""A class handling data logging for a search process and providing methods to visualize that data."""

from __future__ import annotations

from tempfile import TemporaryDirectory
from typing import TYPE_CHECKING, Literal

from .visualize_search_graph import visualize_search_graph

if TYPE_CHECKING:
    import types
    from collections.abc import Callable, MutableMapping

    from ipywidgets import Widget

    from .._compat.typing import Self
    from .visualize_search_graph import SearchNode


class SearchVisualizer:
    """Handling data logging for a search process and providing methods to visualize that data."""

    def __init__(self, data_logging_path: str | None = None) -> None:
        """Handling data logging for a search process and providing methods to visualize that data.

        Args:
            data_logging_path: Path to an empty directory, in which the search process should log all data.
                Defaults to None, in which case a temporary folder will be created.
        """
        if data_logging_path is not None:
            self.data_logging_path: str | None = data_logging_path
            self.data_logging_tmp_dir: TemporaryDirectory[str] | None = None
        else:
            self.data_logging_tmp_dir = TemporaryDirectory()
            self.data_logging_path = self.data_logging_tmp_dir.name

    def __enter__(self) -> Self:
        """Just enables the use of SearchVisualizer in a with statement."""
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: types.TracebackType | None,
    ) -> None:
        """Closes the SearchVisualizer after a with statement."""
        self.close()

    def close(self) -> None:
        """Cleans up the data logging directory, if it is was temporarily created."""
        if self.data_logging_tmp_dir is not None:
            self.data_logging_tmp_dir.cleanup()
            self.data_logging_path = None

    def visualize_search_graph(
        self,
        layer: int | Literal["interactive"] = "interactive",
        architecture_node_positions: MutableMapping[int, tuple[float, float]] | None = None,
        architecture_layout: Literal["dot", "neato", "fdp", "sfdp", "circo", "twopi", "osage", "patchwork"] = "sfdp",
        search_node_layout: Literal[
            "walker", "dot", "neato", "fdp", "sfdp", "circo", "twopi", "osage", "patchwork"
        ] = "walker",
        search_graph_border: float = 0.05,
        architecture_border: float = 0.05,
        swap_arrow_spacing: float = 0.05,
        swap_arrow_offset: float = 0.05,
        use3d: bool = True,
        projection: Literal["orthographic", "perspective"] = "perspective",
        width: int = 1400,
        height: int = 700,
        draw_search_edges: bool = True,
        search_edges_width: float = 0.5,
        search_edges_color: str = "#888",
        search_edges_dash: str = "solid",
        tapered_search_layer_heights: bool = True,
        show_layout: Literal["hover", "click"] | None = "hover",
        show_swaps: bool = True,
        show_shared_swaps: bool = True,
        show_only_solution_path: bool = False,
        color_valid_mapping: str | None = "green",
        color_final_node: str | None = "red",
        search_node_color: str
        | (Callable[[SearchNode], float] | list[str | Callable[[SearchNode], float]]) = "total_cost",
        prioritize_search_node_color: bool | list[bool] = False,
        search_node_color_scale: str | list[str] = "YlGnBu",
        search_node_invert_color_scale: bool | list[bool] = True,
        search_node_colorbar_title: str | list[str | None] | None = None,
        search_node_colorbar_spacing: float = 0.06,
        search_node_height: str
        | (Callable[[SearchNode], float] | list[str | Callable[[SearchNode], float]]) = "total_cost",
        draw_stems: bool = False,
        stems_width: float = 0.7,
        stems_color: str = "#444",
        stems_dash: str = "solid",
        show_search_progression: bool = True,
        search_progression_step: int = 10,
        search_progression_speed: float = 2,
        plotly_settings: MutableMapping[str, MutableMapping[str, object]] | None = None,
    ) -> Widget:
        """Creates a widget to visualize the search graph.

        Args:
            layer (int | Literal['interactive']): Index of the circuit layer, of which the mapping should be visualized. Defaults to "interactive", in which case a slider menu will be created.
            architecture_node_positions (MutableMapping[int, tuple[float, float]] | None): Mapping from physical qubits to (x, y) coordinates. Defaults to None, in which case architecture_layout will be used to generate a layout.
            architecture_layout (Literal[ 'dot', 'neato', 'fdp', 'sfdp', 'circo', 'twopi', 'osage', 'patchwork' ]): The method to use when layouting the qubit connectivity graph. Defaults to "sfdp".
            search_node_layout (Literal[ 'walker', 'dot', 'neato', 'fdp', 'sfdp', 'circo', 'twopi', 'osage', 'patchwork' ]): The method to use when layouting the search graph. Defaults to "walker".
            search_graph_border (float): Size of the border around the search graph. Defaults to 0.05.
            architecture_border (float): Size of the border around the qubit connectivity graph. Defaults to 0.05.
            swap_arrow_spacing (float): Lateral spacing between arrows indicating swaps on the qubit connectivity graph. Defaults to 0.05.
            swap_arrow_offset (float): Offset of heads and shaft of swap arrows from qubits they are pointing to/from. Defaults to 0.05.
            use3d (bool): If a 3D graph should be used for the search graph using the z-axis to plot data features. Defaults to True.
            projection (Literal['orthographic', 'perspective']): Projection type to use in 3D graphs. Defaults to "perspective".
            width (int): Pixel width of the widget. Defaults to 1400.
            height (int): Pixel height of the widget. Defaults to 700.
            draw_search_edges (bool): If edges between search nodes should be drawn. Defaults to True.
            search_edges_width (float): Width of edges between search nodes. Defaults to 0.5.
            search_edges_color (str): Color of edges between search nodes (in CSS format, i.e. '#rrggbb', '#rgb', 'colorname', etc.). Defaults to "#888".
            search_edges_dash (str): Dashing of search edges (in CSS format, i.e. 'solid', 'dot', 'dash', 'longdash', etc.). Defaults to "solid".
            tapered_search_layer_heights (bool): If search graph tree should progressively reduce the height of each layer. Defaults to True.
            show_layout (Literal['hover', 'click'] | None): If the current qubit layout should be shown on the qubit connectivity graph, when clicking or hovering on a search node or not at all. Defaults to "hover".
            show_swaps (bool): Showing swaps on the connectivity graph. Defaults to True.
            show_shared_swaps (bool): Indicate a shared swap by 1 arrow with 2 heads, otherwise 2 arrows in opposite direction are drawn for the 1 shared swap. Defaults to True.
            show_only_solution_path (bool): If only the final solution path should be shown. Defaults to False.
            color_valid_mapping (str | None): Color to use for search nodes containing a valid qubit layout (in CSS format). Defaults to "green".
            color_final_node (str | None): Color to use for the final solution search node (in CSS format). Defaults to "red".
            search_node_color (str | Callable[[SearchNode], float] | list[str | Callable[[SearchNode], float]]): Color to be used for search nodes. Either a static color (in CSS format) or function mapping a mqt.qmap.visualization.SearchNode to a float value, which in turn gets translated into a color by ``search_node_color_scale`` , or a preset data feature ('total_cost' | 'fixed_cost' | 'heuristic_cost' | 'lookahead_penalty'). In case a 3D search graph is used with multiple points per search node, each point's color can be controlled individually via a list. Defaults to "total_cost".
            prioritize_search_node_color (bool | list[ bool ]): If search_node_color should be prioritized over color_valid_mapping and color_final_node. Defaults to False.
            search_node_color_scale (str | list[str]): Color scale to be used for converting float data features to search node colors. (See https://plotly.com/python/builtin-colorscales/ for valid values). Defaults to "YlGnBu".
            search_node_invert_color_scale (bool | list[bool]): If the color scale should be inverted. Defaults to True.
            search_node_colorbar_title (str | list[str  |  None] | None): Title(s) to be shown next to the colorbar(s). Defaults to None.
            search_node_colorbar_spacing (float): Spacing between multiple colorbars. Defaults to 0.06.
            search_node_height (str | Callable[[SearchNode], float] | list[str | Callable[[SearchNode], float]]): Function mapping a mqt.qmap.visualization.SearchNode to a float value to be used as z-value in 3D search graphs or a preset data feature ('total_cost' | 'fixed_cost' | 'heuristic_cost' | 'lookahead_penalty'). Or a list any of such functions/data features, to draw multiple points per search node. Defaults to "total_cost".
            draw_stems (bool): If a vertical stem should be drawn in 3D search graphs to each search node. Defaults to False.
            stems_width (float): Width of stems in 3D search graphs. Defaults to 0.7.
            stems_color (str): Color of stems in 3D search graphs (in CSS format). Defaults to "#444".
            stems_dash (str): Dashing of stems in 3D search graphs (in CSS format). Defaults to "solid".
            show_search_progression (bool): If the search progression should be animated. Defaults to True.
            search_progression_step (int): Step size (in number of nodes added) of search progression animation. Defaults to 10.
            search_progression_speed (float): Speed of the search progression animation. Defaults to 2.
            plotly_settings (MutableMapping[str, MutableMapping[str, any]] | None): Plotly configuration dictionaries to be passed through. Defaults to None.
        .. code-block:: text

            {
                "layout": settings for plotly.graph_objects.Layout (of subplots figure)
                "arrows": settings for plotly.graph_objects.layout.Annotation
                "stats_legend": settings for plotly.graph_objects.layout.Annotation
                "search_nodes": settings for plotly.graph_objects.Scatter resp. ...Scatter3d
                "search_edges": settings for plotly.graph_objects.Scatter resp. ...Scatter3d
                "architecture_nodes": settings for plotly.graph_objects.Scatter
                "architecture_edges": settings for plotly.graph_objects.Scatter
                "architecture_edge_labels": settings for plotly.graph_objects.Scatter
                "search_xaxis": settings for plotly.graph_objects.layout.XAxis resp. ...layout.scene.XAxis
                "search_yaxis": settings for plotly.graph_objects.layout.YAxis resp. ...layout.scene.YAxis
                "search_zaxis": settings for plotly.graph_objects.layout.scene.ZAxis
                "architecture_xaxis": settings for plotly.graph_objects.layout.XAxis
                "architecture_yaxis": settings for plotly.graph_objects.layout.YAxis
            }

        Raises:
            TypeError: If any of the arguments are invalid.

        Returns:
            Widget: An interactive IPython widget to visualize the search graph.
        """
        if plotly_settings is None:
            plotly_settings = {}
        if self.data_logging_path is None:
            msg = "SearchVisualizer has already been closed and data logs have been discarded."
            raise ValueError(msg)
        return visualize_search_graph(
            data_logging_path=self.data_logging_path,
            layer=layer,
            architecture_node_positions=architecture_node_positions,
            architecture_layout=architecture_layout,
            search_node_layout=search_node_layout,
            search_graph_border=search_graph_border,
            architecture_border=architecture_border,
            swap_arrow_spacing=swap_arrow_spacing,
            swap_arrow_offset=swap_arrow_offset,
            use3d=use3d,
            projection=projection,
            width=width,
            height=height,
            draw_search_edges=draw_search_edges,
            search_edges_width=search_edges_width,
            search_edges_color=search_edges_color,
            search_edges_dash=search_edges_dash,
            tapered_search_layer_heights=tapered_search_layer_heights,
            show_layout=show_layout,
            show_swaps=show_swaps,
            show_shared_swaps=show_shared_swaps,
            show_only_solution_path=show_only_solution_path,
            color_valid_mapping=color_valid_mapping,
            color_final_node=color_final_node,
            search_node_color=search_node_color,
            prioritize_search_node_color=prioritize_search_node_color,
            search_node_color_scale=search_node_color_scale,
            search_node_invert_color_scale=search_node_invert_color_scale,
            search_node_colorbar_title=search_node_colorbar_title,
            search_node_colorbar_spacing=search_node_colorbar_spacing,
            search_node_height=search_node_height,
            draw_stems=draw_stems,
            stems_width=stems_width,
            stems_color=stems_color,
            stems_dash=stems_dash,
            show_search_progression=show_search_progression,
            search_progression_step=search_progression_step,
            search_progression_speed=search_progression_speed,
            plotly_settings=plotly_settings,
        )
