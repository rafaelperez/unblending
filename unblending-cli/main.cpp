#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <iostream>
#include <unblending/unblending.hpp>
#include <unblending/equations.hpp>
#include <unblending/CLI11.hpp>

using namespace unblending;

int main(int argc, char** argv)
{
    CLI::App app("Unblending-cli - A command line interface (CLI) for using the ``unblending'' library.");

    std::string output_directory_path = "./out";
    app.add_option("-o,--outdir", output_directory_path, "Path to the output directory");
    
    int width;
    app.add_option("-w,--width", width, "Target width (pixels) of the output image (default: original resolution)");

    bool use_explicit_name = false;
    app.add_flag("-e,--explicit-mode-names", use_explicit_name, "Append blend mode names to output image file names");
    
    bool export_verbosely = false;
    app.add_flag("-v,--verbose-export", export_verbosely, "Export intermediate files as well as final outcomes");
    
    std::string image_file_path;
    app.add_option("-i,--input-image-path", image_file_path, "Path to the input image (png or jpg)") -> required();

    std::string layer_infos_path;
    app.add_option("-l,--layer-infos-path", layer_infos_path, "Path to the layer infos (json)") -> required();

    bool has_opaque_background = false;
    app.add_flag("--opaque-bg", has_opaque_background, "Set the first layer to have a opaque or semi-transparent alpha. Set to `false` for getting Aksoy similar results.");

    bool force_smooth_background = false;
    app.add_flag("--smooth-bg", force_smooth_background, "Run in the guided filtering also on the first color layer.");

    CLI11_PARSE(app, argc, argv);

    if (std::system(("mkdir -p " + output_directory_path).c_str()) < 0) { exit(1); };
    
    // Import the target image and resize it if a target width is specified
    const ColorImage original_image = [&]()
    {
        const ColorImage image(image_file_path);
        const bool use_target_width = (app.count("--width") == 1);
        return use_target_width ? image.get_scaled_image(width) : image;
    }();
    
    // Prepare layer infos
    std::vector<LayerInfo> layer_infos = import_layer_infos(layer_infos_path);
    
    // Compute color unmixing to obtain an initial result
    const std::vector<ColorImage> layers = compute_color_unmixing(original_image, layer_infos, has_opaque_background);
    
    // Perform post processing steps
    const std::vector<ColorImage> refined_layers = perform_matte_refinement(original_image, layers, layer_infos, has_opaque_background, force_smooth_background);
    
    // Export layers
    std::cout << export_verbosely << std::endl;
    if (export_verbosely) { export_layers(layers, output_directory_path, "non-smoothed-layer", true, use_explicit_name, layer_infos); }
    export_layers(refined_layers, output_directory_path, "layer", export_verbosely, use_explicit_name, layer_infos);
    
    // Export the original image
    if (export_verbosely) { original_image.save(output_directory_path + "/input.png"); }
    
    // Export the composited image
    const auto modes    = extract_blend_modes(layer_infos);
    const auto comp_ops = extract_comp_ops(layer_infos);
    
    const ColorImage composited_image = composite_layers(layers, comp_ops, modes);
    if (export_verbosely) { composited_image.save(output_directory_path + "/non-smoothed-recomposited.png"); }
    
    const ColorImage refined_composited_image = composite_layers(refined_layers, comp_ops, modes);
    if (export_verbosely) { refined_composited_image.save(output_directory_path + "/recomposited.png"); }
    
    // Export visualizations of color models
    if (export_verbosely)
    {
        const std::vector<ColorModelPtr> models = extract_color_models(layer_infos);
        export_models(models, output_directory_path, "model");
    }
    
    // Export layer infos
    export_layer_infos(layer_infos, output_directory_path);
    
    return 0;
}
