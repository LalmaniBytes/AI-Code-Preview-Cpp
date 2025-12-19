    #define CPPHTTPLIB_OPENSSL_SUPPORT
    #include "httplib.h"
    #include "json.hpp"
    #include <iostream>
    #include <string>
    #include <fstream>
    #include <sstream>
    #include <algorithm> // Needed for cleaning the key

    using json = nlohmann::json;

    // --- CONFIGURATION ---
    // PASTE YOUR KEY INSIDE THE QUOTES
    const std::string RAW_API_KEY = "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"; 

    const std::string OR_HOST = "openrouter.ai";
    const std::string OR_PATH = "/api/v1/chat/completions";
    // You can switch this to "google/gemini-2.0-flash-exp:free" or "google/gemini-2.0-flash-thinking-exp:free"
    const std::string MODEL_ID = "deepseek/deepseek-r1-0528:free";

    // HELPER: Removes spaces/newlines from the start and end of the key
    std::string clean_key(std::string s) {
        s.erase(0, s.find_first_not_of(" \n\r\t"));
        s.erase(s.find_last_not_of(" \n\r\t") + 1);
        return s;
    }

    int main() {
        httplib::Server svr;

        // 1. Serve Frontend
        svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
            std::ifstream file("index.html");
            std::stringstream buffer;
            if(file) {
                buffer << file.rdbuf();
                res.set_content(buffer.str(), "text/html");
            } else {
                res.set_content("<h1>Error: index.html not found</h1>", "text/html");
            }
        });

        // 2. Handle Analysis
        svr.Post("/analyze", [](const httplib::Request& req, httplib::Response& res) {
            try {
                // Clean the key immediately
                std::string final_key = clean_key(RAW_API_KEY);

                auto json_req = json::parse(req.body);
                std::string user_code = json_req["code"];
                
                std::cout << "\n[INFO] ------------------------------------------------" << std::endl;
                std::cout << "[INFO] Sending to OpenRouter..." << std::endl;
                
                // DEBUG: Check if the key format looks correct (Don't share this screenshot publicly if it shows full key)
                std::cout << "[DEBUG] Auth Header being sent: Bearer " << final_key.substr(0, 10) << "..." << std::endl;

                httplib::SSLClient cli(OR_HOST);
                cli.enable_server_certificate_verification(false);
                
                httplib::Headers headers = {
                    {"Authorization", "Bearer " + final_key},
                    {"Content-Type", "application/json"},
                    {"HTTP-Referer", "http://localhost:8080"},
                    {"X-Title", "CodePreviewApp"}
                };

                json payload = {
                    {"model", MODEL_ID},
                    {"messages", {
                        {
                            {"role", "user"},
                            {"content", "Review the following code. Provide corrections and improvements:\n\n . Give the recommended Youtube Video link for the relevant topic " + user_code}
                        }
                    }}
                };

                auto api_res = cli.Post(OR_PATH, headers, payload.dump(), "application/json");

                std::string result_text;

                if (api_res && api_res->status == 200) {
                    try {
                        auto response_json = json::parse(api_res->body);
                        result_text = response_json["choices"][0]["message"]["content"];
                        std::cout << "[SUCCESS] AI Responded!" << std::endl;
                    } catch(const std::exception& e) {
                        std::cout << "[ERROR] JSON Parse Error: " << e.what() << std::endl;
                        result_text = "Error parsing AI response.";
                    }
                } else {
                    std::cout << "[ERROR] Status: " << (api_res ? std::to_string(api_res->status) : "Timeout") << std::endl;
                    if(api_res) std::cout << "[ERROR] Body: " << api_res->body << std::endl;
                    result_text = "AI Error: " + (api_res ? std::to_string(api_res->status) : "Connection Failed");
                }
                
                std::cout << "[INFO] ------------------------------------------------" << std::endl;

                json response_data;
                response_data["result"] = result_text;
                res.set_content(response_data.dump(), "application/json");

            } catch (...) {
                res.status = 500;
                res.set_content("{\"result\": \"Internal Server Error\"}", "application/json");
            }
        });

        std::cout << "Server started at http://localhost:8080" << std::endl;
        svr.listen("0.0.0.0", 8080);
        return 0;
    }