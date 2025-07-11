#include "cxxopts.hpp"
#include "deejai/generator.hpp"
#include "deejai/scanner.hpp"
#include "deejai/utils.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define MAX_ARGS 2000

static int error_exit_main(const char *error) {
    std::cerr << "Error: " << error << "\nUse --help for usage.\n";
    return 1;
}

static std::vector<std::string> get_vector_option(const cxxopts::ParseResult &result, const std::string &key) {
    std::vector<std::string> vec;
    for (auto &opt : result) {
        if (opt.key() == key) {
            vec.push_back(opt.value());
        }
    }
    return vec;
}

std::vector<std::string> parse_args(const std::string &filename) {
    std::ifstream file(filename);
    std::string input = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::vector<std::string> args;

    bool in_quotes = false;
    char quote_char = 0;
    std::string current;
    for (char c : input) {
        if ((c == '"' || c == '\'') && !in_quotes) {
            in_quotes = true;
            quote_char = c;
        } else if (c == quote_char && in_quotes) {
            in_quotes = false;
        } else if (std::isspace(c) && !in_quotes) {
            if (!current.empty()) {
                args.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty())
        args.push_back(current);
    return args;
}

int main(int argc, char *argv[]) {
    const char *new_argv[MAX_ARGS];
    int new_argc = 0;

    std::vector<std::string> file_args;
    new_argv[new_argc++] = argv[0];
    if (argc > 1 && argv[1][0] == '@') {
        file_args = parse_args(argv[1] + 1);
        for (const auto &arg : file_args) {
            new_argv[new_argc++] = arg.c_str();
        }
    } else {
        for (int i = 1; i < argc; i++) {
            new_argv[new_argc++] = argv[i];
        }
    }

    try {
        // Identation is messed up due to clang-format, too lazy to fix it
        cxxopts::Options options("deej-ai", "Tool for generating playlists.\n"
                                            "A scan of the music library must first be completed.\n"
                                            "The generation option can then be used to create playlists based on input songs from that library.\n\n"
                                            "Usage:\n"
                                            "  deej-ai --scan <path1> --scan <path2> --model <path> --vec-dir <path> [options]\n"
                                            "  deej-ai --generate <method> --input <song1> --input <song2> ... --vec-dir <path> [options]\n"
                                            "  deej-ai --reorder --input <song1> --input <song2> ... --vec-dir <path> [options]\n\n"
                                            "At least one of --scan, --generate or --reorder must be used.\n");
        options.add_options()("h,help", "Show help");
        options.add_options()("scan", "Scan mode. Requires one or more scan paths.\n", cxxopts::value<std::string>());
        options.add_options()("generate", "Generate mode. Requires the method ('append', 'connect' or 'cluster').\n\n"
                                          "-'append': Appends songs at the end of the input, taking into account the last n-songs specified by the 'lookback'.\n\n"
                                          "-'connect': Connects the input songs (if only one song is provided, 'append' will be used instead.)\n\n"
                                          "-'cluster': Appends songs at the end of the input, taking into account the original input songs only.\n",
                              cxxopts::value<std::string>());
        options.add_options()("reorder", "Reorder mode. Creates a playlist by reordering the input songs to improve the listening experience.");
        options.add_options("Common")("d,vec-dir", "Directory of cached vectors.",
                                      cxxopts::value<std::string>());
        options.add_options("Scan")("m,model", "Path to the model file.",
                                    cxxopts::value<std::string>());
        options.add_options("Scan")("ffmpeg", "Path to the ffmpeg library.",
                                    cxxopts::value<std::string>()->default_value("ffmpeg"));
        options.add_options("Scan")("b,batch-size", "Batch size.",
                                    cxxopts::value<int>()->default_value("100"));
        options.add_options("Scan")("e,epsilon", "Epsilon value.",
                                    cxxopts::value<double>()->default_value("0.001"));
        options.add_options("Generate & Reorder")("i,input", "Input song path. This flag can be used multiple times.",
                                                  cxxopts::value<std::string>());
        options.add_options("Generate & Reorder")("o,m3u-out", "The m3u filepath to save the playlist. "
                                                               "If no file is specified, the output will be printed instead.",
                                                  cxxopts::value<std::string>()->default_value(""));
        options.add_options("Generate")("nsongs", "Number of songs in the playlist. (The number of songs connecting the inputs in 'connect' method.)",
                                        cxxopts::value<int>()->default_value("10"));
        options.add_options("Generate")("noise", "Noise level. Higher noise will result in greater randomness. "
                                                 "Preferably use values between 0 and 1.",
                                        cxxopts::value<float>()->default_value("0.0"));
        options.add_options("Generate")("l,lookback", "The lookback to pick the next song.",
                                        cxxopts::value<int>()->default_value("3"));
        options.add_options("Generate")("reorder-output", "Use reorder on the generation output.");
        options.add_options("Reorder")("first", "The desired first song of the reordered playlist.",
                                       cxxopts::value<std::string>());

        auto result = options.parse(new_argc, new_argv);

        if (result.count("help") || argc == 1) {
            std::cout << options.help({"", "Common", "Scan", "Generate & Reorder", "Generate", "Reorder"}) << std::endl;
            return 0;
        }

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

        bool isScan = result.count("scan");
        bool isGenerate = result.count("generate");
        bool isReorder = result.count("reorder");

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

        if (isReorder) {
            if (isGenerate) {
                return error_exit_main("--reorder cannot be used with --generate use --reorder-output instead");
            }

            if (!result.count("input") || !result.count("vec-dir")) {
                return error_exit_main("--reorder requires --input and --vec-dir");
            }
        }

        if (!isScan && !isGenerate && !isReorder) {
            return error_exit_main("Either --scan, --generate or --reorder must be used");
        }

        if (isScan) {
            deejai::utils::FFMPEG_PATH = result["ffmpeg"].as<std::string>();
            std::string vec_dir = result["vec-dir"].as<std::string>();
            std::vector<std::string> scan_inputs = get_vector_option(result, "scan");
            std::string model = result["model"].as<std::string>();
            int batch_size = result["batch-size"].as<int>();
            double epsilon = result["epsilon"].as<double>();

            deejai::scanner deejai_scanner(model, vec_dir);
            deejai_scanner.set_batch_size(batch_size);
            deejai_scanner.set_epsilon(epsilon);
            if (deejai_scanner.scan(scan_inputs)) {
                std::cout << "Scan completed successfully." << std::endl;
            }
        }

        if (isGenerate) {
            std::string method = result.count("generate") ? result["generate"].as<std::string>() : "";
            std::string vec_dir = result["vec-dir"].as<std::string>();
            std::vector<std::string> input_songs = get_vector_option(result, "input");
            int nsongs = result["nsongs"].as<int>();
            float noise = result["noise"].as<float>();
            int lookback = result["lookback"].as<int>();
            bool reorder_output = result.count("reorder-output");

            std::string m3u_file = result["m3u-out"].as<std::string>();

            deejai::generator gen(vec_dir);
            auto ret = gen.generate_playlist(method, input_songs, nsongs, lookback, noise);
            if (reorder_output) {
                ret = gen.reorder(ret);
            }
            if (m3u_file.empty()) {
                for (const auto &file : ret) {
                    std::cout << file << std::endl;
                }
            } else {
                deejai::utils::save_as_m3u(m3u_file, ret);
            }
        }

        if (isReorder) {
            std::string vec_dir = result["vec-dir"].as<std::string>();
            std::vector<std::string> input_songs = get_vector_option(result, "input");
            std::string m3u_file = result["m3u-out"].as<std::string>();
            std::string first_song = result.count("first") ? result["first"].as<std::string>() : "";

            deejai::generator gen(vec_dir);
            auto ret = gen.reorder(input_songs, first_song);
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
