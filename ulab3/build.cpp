// &Output: dedup_hardlinks
#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <filesystem>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <openssl/sha.h>

namespace fs = std::filesystem;

std::string compute_sha1(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Предупреждение: не удалось открыть '" << path << "' для чтения (пропускаем)\n";
        return "";
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return "";
    }

    if (!S_ISREG(st.st_mode)) {
        close(fd);
        return "";
    }

    SHA_CTX ctx;
    if (!SHA1_Init(&ctx)) {
        close(fd);
        std::cerr << "Ошибка: SHA1_Init не удался для '" << path << "'\n";
        return "";
    }

    constexpr size_t BUF_SIZE = 8192;
    std::vector<unsigned char> buffer(BUF_SIZE);
    ssize_t n;

    while ((n = read(fd, buffer.data(), BUF_SIZE)) > 0) {
        if (!SHA1_Update(&ctx, buffer.data(), n)) {
            close(fd);
            std::cerr << "Ошибка: SHA1_Update не удался для '" << path << "'\n";
            return "";
        }
    }

    if (n == -1) {
        close(fd);
        std::cerr << "Ошибка чтения из '" << path << "'\n";
        return "";
    }

    unsigned char hash[SHA_DIGEST_LENGTH];
    if (!SHA1_Final(hash, &ctx)) {
        close(fd);
        std::cerr << "Ошибка: SHA1_Final не удался для '" << path << "'\n";
        return "";
    }
    close(fd);

    char hex[SHA_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        snprintf(&hex[i * 2], 3, "%02x", hash[i]);
    }
    return std::string(hex, SHA_DIGEST_LENGTH * 2);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <каталог>\n";
        return 1;
    }

    std::string root_path = argv[1];
    if (!fs::exists(root_path)) {
        std::cerr << "Ошибка: каталог '" << root_path << "' не существует.\n";
        return 2;
    }
    if (!fs::is_directory(root_path)) {
        std::cerr << "Ошибка: '" << root_path << "' не является каталогом.\n";
        return 3;
    }

    std::unordered_map<std::string, std::string> hash_to_first_path;
    int files_processed = 0;
    int links_created = 0;

    std::cout << "Сканирование каталога: " << root_path << "\n";

    try {
        for (const auto& entry : fs::recursive_directory_iterator(root_path)) {
            if (!entry.is_regular_file()) continue;

            std::string path = entry.path().string();
            std::string hash = compute_sha1(path);
            if (hash.empty()) continue;

            files_processed++;

            auto it = hash_to_first_path.find(hash);
            if (it == hash_to_first_path.end()) {
                hash_to_first_path[hash] = path;
                std::cout << "[+] Хеш " << hash << " → сохранён как оригинал: " << path << "\n";
            } else {
                const std::string& first_path = it->second;

                struct stat st_orig, st_curr;
                if (stat(first_path.c_str(), &st_orig) != 0 || stat(path.c_str(), &st_curr) != 0) {
                    std::cerr << "Предупреждение: не удалось получить stat для '" << path << "' или '" << first_path << "'\n";
                    continue;
                }

                if (st_orig.st_dev == st_curr.st_dev && st_orig.st_ino == st_curr.st_ino) {
                    std::cout << "[=] '" << path << "' уже hard link на '" << first_path << "' — пропуск.\n";
                    continue;
                }

                if (unlink(path.c_str()) != 0) {
                    std::cerr << "Ошибка: не удалось удалить '" << path << "' перед созданием hard link\n";
                    continue;
                }

                if (link(first_path.c_str(), path.c_str()) != 0) {
                    std::cerr << "Ошибка: не удалось создать hard link от '" << first_path << "' к '" << path << "'\n";
                    continue;
                }
                links_created++;
                std::cout << "[→] '" << path << "' заменён hard link на '" << first_path << "'\n";
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Ошибка обхода каталога: " << e.what() << "\n";
        return 4;
    }

    std::cout << "\nГотово\n"
              << "Обработано файлов: " << files_processed << "\n"
              << "Создано жёстких ссылок: " << links_created << "\n";

    return 0;
}