#include "config.h"

#include <fstream>

Config config;

std::filesystem::path getSystemConfigPath()
{
    // C:/ProgramData
    std::filesystem::path program_data_dir = std::getenv("ALLUSERSPROFILE");
    if (program_data_dir.empty()) {
        throw std::runtime_error("failed to get ALLUSERSPROFILE path from environment variables");
    }

    // C:/ProgramData/NikonTiControl
    std::filesystem::path app_dir = program_data_dir / "NikonTiControl";
    if (!std::filesystem::exists(app_dir)) {
        throw std::runtime_error(fmt::format("Directory {} does not exists. It needs to be created manually and assigned with the correct permission.", app_dir.string()));
    }

    return app_dir / "config.json";
}

std::filesystem::path getUserConfigPath()
{
    // C:/Users/<username>/AppData/Roaming
    std::filesystem::path user_app_data_dir = std::getenv("APPDATA");
    if (user_app_data_dir.empty()) {
        throw std::runtime_error("failed to get APPDATA path from environment variables");
    }

    // C:/Users/<username>/AppData/Roaming/NikonTiControl
    std::filesystem::path user_app_dir = user_app_data_dir / "NikonTiControl";
    if (!std::filesystem::exists(user_app_dir)) {
        std::filesystem::create_directory(user_app_dir);
    }

    return user_app_dir / "user.json";
}

void loadSystemConfig(std::filesystem::path filename)
{
    std::ifstream ifs(filename.string());
    nlohmann::json j = nlohmann::json::parse(ifs);

    j.at("unet_model").get_to(config.system.unet_model);
    j.at("pixel_size").get_to(config.system.pixel_size);

    try {
        std::map<std::string, std::map<std::string, Label>> m_labels;
        j.at("labels").get_to(m_labels);
        for (const auto& [k, v] : m_labels) {
            config.system.labels[k] = v;
        }
    } catch (std::exception &e) {
        throw std::runtime_error(fmt::format("labels: {}", e.what()));
    }
    
    try {
        j.at("presets").get_to(config.system.presets);
    } catch (std::exception &e) {
        throw std::runtime_error(fmt::format("presets: {}", e.what()));
    }

    return;
}

void loadUserConfig(std::filesystem::path filename)
{
    std::ifstream ifs(filename.string());
    json j = json::parse(ifs);

    config.user.name = j.at("name").get<std::string>();
    config.user.email = j.at("email").get<std::string>();
    config.user.data_root = j.at("data_root").get<std::string>();
}
