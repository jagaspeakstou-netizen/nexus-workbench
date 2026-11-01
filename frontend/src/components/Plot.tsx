// react-plotly.js's default export expects the full "plotly.js" package
// as a peer dependency; we use the much smaller plotly.js-dist-min
// instead, so it's wired through the library's documented factory
// function rather than the default export.
import Plotly from "plotly.js-dist-min";
import createPlotlyComponent from "react-plotly.js/factory";

const Plot = createPlotlyComponent(Plotly);
export default Plot;
