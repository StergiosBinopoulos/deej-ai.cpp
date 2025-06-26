#include "cxxopts.hpp"
#include "deejai/generator.hpp"
#include "deejai/scanner.hpp"
#include "deejai/utils.hpp"

#include <iostream>
#include <string>
#include <vector>

static int error_exit_main(const char *error) {
    std::cerr << "Error: " << error << "\nUse --help for usage.\n";
    return 1;
}

int main(int argc, char *argv[]) {
    try {
        // Identation is messed up due to clang-format, too lazy to fix it
        cxxopts::Options options("deej-ai", "Tool for generating playlists.\n"
                                            "A scan of the music library must first be completed.\n"
                                            "The generation option can then be used to create playlists based on input songs from that library.\n\n"
                                            "Usage:\n"
                                            "  deej-ai --scan <paths> --model <path> --vec-dir <path> [options]\n"
                                            "  deej-ai --generate <method> --input <song1> --input <song2> ... --vec-dir <path> [options]\n\n"
                                            "Exactly one of --scan or --generate must be used.\n");
        options.add_options()("h,help", "Show help");
        options.add_options()("scan", "Scan mode. Requires one or more scan paths.", cxxopts::value<std::vector<std::string>>());
        options.add_options()("generate", "Generate mode. Requires the method ('append', 'connect' or 'cluster').\n\n"
                                          "-'append': Appends songs at the end of the input, taking into account the last n-songs specified by the 'lookback'.\n\n"
                                          "-'connect': Connects the input songs (if only one song is provided, 'append' will be used instead.)\n\n"
                                          "-'cluster': Appends songs at the end of the input, taking into account the original input songs only.",
                              cxxopts::value<std::string>());
        options.add_options("Common")("d,vec-dir", "Directory of cached vectors.",
                                      cxxopts::value<std::string>());
        options.add_options("Scan")("m,model", "Path to the model file.",
                                    cxxopts::value<std::string>());
        options.add_options("Scan")("b,batch-size", "Batch size.",
                                    cxxopts::value<int>()->default_value("100"));
        options.add_options("Scan")("e,epsilon", "Epsilon value.",
                                    cxxopts::value<double>()->default_value("0.001"));
        options.add_options("Generate")("i,input", "Input song path. This flag can be used multiple times.",
                                        cxxopts::value<std::vector<std::string>>());
        options.add_options("Generate")("nsongs", "Number of songs in the playlist. (The number of songs connecting the inputs in 'connect' method.)",
                                        cxxopts::value<int>()->default_value("10"));
        options.add_options("Generate")("noise", "Noise level. Higher noise will result in greater randomness. "
                                                 "Preferably use values between 0 and 1.",
                                        cxxopts::value<float>()->default_value("0.0"));
        options.add_options("Generate")("l,lookback", "The lookback to pick the next song.",
                                        cxxopts::value<int>()->default_value("3"));
        options.add_options("Generate")("o,m3u-out", "The m3u filepath to save the playlist. "
                                                     "If no file is specified, the output will be printed instead.",
                                        cxxopts::value<std::string>()->default_value(""));

        auto result = options.parse(argc, argv);

        const auto &unrec = result.unmatched();
        if (!unrec.empty()) {
            std::ostringstream oss;
            oss << "Unrecognized arguments:";
            for (const auto &arg : unrec) {
                oss << " " << arg;
            }
            throw cxxopts::exceptions::exception(oss.str());
        }

        std::unordered_set<std::string> options_set;
        std::unordered_set<std::string> non_unique_options = {"input", "scan"};
        for (const auto &opt : result.arguments()) {
            const std::string key = opt.key();
            if (!options_set.contains(key)) {
                options_set.insert(key);
            } else if (!non_unique_options.contains(key)) {
                const std::string message = key + " option is unique.";
                return error_exit_main(message.c_str());
            }
        }

        if (result.count("help") || argc == 1) {
            std::cout << options.help({"", "Common", "Scan", "Generate"}) << std::endl;
            return 0;
        }

        bool isScan = result.count("scan");
        bool isGenerate = result.count("generate");

        if (isScan) {
            if (!result.count("scan") || !result.count("model") || !result.count("vec-dir")) {
                return error_exit_main("--scan requires --model, --vec-dir, and one or more scan inputs");
            }
        }

        if (isGenerate) {
            std::string method = result.count("generate") ? result["generate"].as<std::string>() : "";

            if (!method.empty() && method != "connect" && method != "append" && method != "cluster") {
                return error_exit_main("--generate method must be one of: append, connect, cluster");
            }
            if (!result.count("input") || !result.count("vec-dir")) {
                return error_exit_main("--generate requires --input and --vec-dir");
            }
        }

        if (!isScan && !isGenerate) {
            return error_exit_main("Either --scan or --generate must be used");
        }

        if (isScan) {
            std::string vec_dir = result["vec-dir"].as<std::string>();
            std::vector<std::string> scan_inputs = result["scan"].as<std::vector<std::string>>();
            std::string model = result["model"].as<std::string>();
            int batch_size = result["batch-size"].as<int>();
            double epsilon = result["epsilon"].as<double>();

            deejai::scanner deejai_scanner(model, vec_dir);
            deejai_scanner.set_batch_size(batch_size);
            deejai_scanner.set_epsilon(epsilon);
            deejai_scanner.scan(scan_inputs);
        }

        if (isGenerate) {
            std::string method = result.count("generate") ? result["generate"].as<std::string>() : "";
            std::string vec_dir = result["vec-dir"].as<std::string>();
            std::vector<std::string> input_songs = result["input"].as<std::vector<std::string>>();
            int nsongs = result["nsongs"].as<int>();
            float noise = result["noise"].as<float>();
            int lookback = result["lookback"].as<int>();
            std::string m3u_file = result["m3u-out"].as<std::string>();

            deejai::generator gen(vec_dir);
            auto ret = gen.generate_playlist(method, input_songs, nsongs, lookback, noise);
            if (m3u_file.empty()) {
                for (const auto &file : ret) {
                    std::cout << file << std::endl;
                }
            } else {
                deejai::utils::save_as_m3u(m3u_file, ret);
            }
        }
    } catch (const cxxopts::exceptions::exception &exception) {
        return error_exit_main(exception.what());
    }
}
