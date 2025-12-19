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
// IMPORTANT: REPLACE THIS WITH YOUR GOOGLE GEMINI API KEY
const std::string RAW_API_KEY = "AIzaSyAapUPmbTpEdEXgZ2j5I-AcM7OklGcFLPg"; 

// Google Gemini Host and Endpoint
const std::string GOOGLE_HOST = "generativelanguage.googleapis.com";
const std::string GOOGLE_PATH = "/v1beta/models/gemini-2.5-flash:generateContent";

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

    // 2. Handle Analysis (Using Google Gemini)
    svr.Post("/analyze", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Clean the key immediately
            std::string final_key = clean_key(RAW_API_KEY);

            auto json_req = json::parse(req.body);
            std::string user_code = json_req["code"];
            
            std::cout << "\n[INFO] ------------------------------------------------" << std::endl;
            std::cout << "[INFO] Sending to Google Gemini..." << std::endl;
            
            httplib::SSLClient cli(GOOGLE_HOST);
            // Google certificates are usually trusted, but we keep this false for local dev simplicity
            cli.enable_server_certificate_verification(false); 
            
            // Google uses 'x-goog-api-key' header or query param. 
            // We use the header for cleaner code.
            httplib::Headers headers = {
                {"x-goog-api-key", final_key},
                {"Content-Type", "application/json"}
            };

            // Gemini JSON Structure: { "contents": [{ "parts": [{ "text": "..." }] }] }
            json payload = {
                {"contents", {
                    {
                        {"parts", {
                            {
                                {"text", "Review the following code. Provide corrections and improvements:\n\n . Give the recommended Youtube Video link for the relevant topic " + user_code}
                            }
                        }}
                    }
                }}
            };

            auto api_res = cli.Post(GOOGLE_PATH, headers, payload.dump(), "application/json");

            std::string result_text;

            if (api_res && api_res->status == 200) {
                try {
                    auto response_json = json::parse(api_res->body);
                    // Gemini Response Structure: candidates[0].content.parts[0].text
                    if (response_json.contains("candidates") && !response_json["candidates"].empty()) {
                        result_text = response_json["candidates"][0]["content"]["parts"][0]["text"];
                        std::cout << "[SUCCESS] Gemini Responded!" << std::endl;
                    } else {
                        result_text = "Error: No candidates returned by Gemini.";
                        std::cout << "[ERROR] Empty candidates in response." << std::endl;
                    }
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