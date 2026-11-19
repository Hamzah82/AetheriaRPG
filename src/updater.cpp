// updater.cpp - safe in-game git pull and rebuild
#include "updater.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace rpg {

namespace {

namespace fs = std::filesystem;

constexpr size_t kMaxOutput = 8000;

#ifdef _WIN32
constexpr const char* kExeSuffix = ".exe";
#else
constexpr const char* kExeSuffix = "";
#endif

std::string trimCopy(std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.erase(0, 1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    return text;
}

std::string readFirstLine(const fs::path& path) {
    std::ifstream file(path);
    if (!file) return {};
    std::string line;
    std::getline(file, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return trimCopy(line);
}

std::string currentExecutablePath() {
#ifdef _WIN32
    std::vector<char> buffer(MAX_PATH);
    for (;;) {
        DWORD n = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (n == 0) return {};
        if (n < buffer.size()) return std::string(buffer.data(), n);
        buffer.resize(buffer.size() * 2);
    }
#else
    char buffer[4096];
    ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (n <= 0) return {};
    buffer[n] = '\0';
    return buffer;
#endif
}

std::string findExecutablePath(const std::string& program) {
#ifdef _WIN32
    const bool pathLike = program.find('/') != std::string::npos ||
                          program.find('\\') != std::string::npos ||
                          (program.size() > 1 && program[1] == ':');
    std::error_code ec;
    if (pathLike) {
        return fs::is_regular_file(program, ec) ? program : std::string();
    }
    std::vector<char> found(MAX_PATH * 2);
    DWORD n = SearchPathA(nullptr, program.c_str(), ".exe",
                          static_cast<DWORD>(found.size()), found.data(), nullptr);
    if (n > 0 && n < found.size()) return std::string(found.data(), n);
    return {};
#else
    if (program.find('/') != std::string::npos) {
        return access(program.c_str(), X_OK) == 0 ? program : std::string();
    }
    const char* pathEnv = std::getenv("PATH");
    std::string paths = pathEnv ? pathEnv : "/usr/local/bin:/usr/bin:/bin";
    size_t start = 0;
    while (start <= paths.size()) {
        size_t end = paths.find(':', start);
        std::string dir = paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (dir.empty()) dir = ".";
        std::string candidate = dir + "/" + program;
        if (access(candidate.c_str(), X_OK) == 0) {
            std::error_code ec;
            if (!fs::is_directory(candidate, ec)) return candidate;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
#endif
}

#ifndef _WIN32
void writeStderr(const char* message) {
    const size_t length = std::strlen(message);
    size_t offset = 0;
    while (offset < length) {
        ssize_t n = write(STDERR_FILENO, message + offset, length - offset);
        if (n <= 0) break;
        offset += static_cast<size_t>(n);
    }
}
#endif

void appendCapped(std::string& output, const char* data, size_t size) {
    if (!data || size == 0) return;
    output.append(data, size);
    if (output.size() > kMaxOutput) {
        output.erase(0, output.size() - kMaxOutput);
    }
}

#ifdef _WIN32
// Quote one argument using the rules the Microsoft C runtime expects when it
// parses a command line back into argv.
std::string quoteWindowsArg(const std::string& arg) {
    if (arg.empty()) return "\"\"";
    if (arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;
    std::string out = "\"";
    for (auto it = arg.begin();; ++it) {
        size_t backslashes = 0;
        while (it != arg.end() && *it == '\\') {
            ++it;
            ++backslashes;
        }
        if (it == arg.end()) {
            out.append(backslashes * 2, '\\');
            break;
        }
        if (*it == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
        } else {
            out.append(backslashes, '\\');
            out.push_back(*it);
        }
    }
    out.push_back('"');
    return out;
}

int runCommand(const std::string& cwd, const std::vector<std::string>& args,
               std::string* output) {
    if (args.empty()) return 127;
    std::string program = findExecutablePath(args[0]);
    if (program.empty()) {
        if (output) *output += "Perintah tidak ditemukan: " + args[0] + "\n";
        return 127;
    }

    std::string commandLine = quoteWindowsArg(program);
    for (size_t i = 1; i < args.size(); ++i) {
        commandLine += " ";
        commandLine += quoteWindowsArg(args[i]);
    }

    SECURITY_ATTRIBUTES security;
    std::memset(&security, 0, sizeof(security));
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) return 127;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    HANDLE nullInput = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &security, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOA startup;
    std::memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = nullInput != INVALID_HANDLE_VALUE ? nullInput
                                                          : GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION process;
    std::memset(&process, 0, sizeof(process));
    std::vector<char> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back('\0');
    BOOL started = CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr,
                                  cwd.empty() ? nullptr : cwd.c_str(), &startup, &process);
    CloseHandle(writePipe);
    if (nullInput != INVALID_HANDLE_VALUE) CloseHandle(nullInput);
    if (!started) {
        CloseHandle(readPipe);
        if (output) *output += "Gagal menjalankan perintah: " + args[0] + "\n";
        return 127;
    }

    char buffer[4096];
    DWORD got = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &got, nullptr) && got > 0) {
        if (output) appendCapped(*output, buffer, got);
    }
    CloseHandle(readPipe);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 127;
    if (!GetExitCodeProcess(process.hProcess, &exitCode)) exitCode = 127;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}
#else
int runCommand(const std::string& cwd, const std::vector<std::string>& args,
               std::string* output) {
    if (args.empty()) return 127;
    std::string program = findExecutablePath(args[0]);
    if (program.empty()) {
        if (output) *output += "Perintah tidak ditemukan: " + args[0] + "\n";
        return 127;
    }

    std::vector<std::string> owned;
    owned.reserve(args.size());
    owned.push_back(program);
    for (size_t i = 1; i < args.size(); ++i) owned.push_back(args[i]);
    std::vector<char*> argv;
    argv.reserve(owned.size() + 1);
    for (std::string& arg : owned) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);

    int fds[2];
    if (pipe(fds) != 0) return 127;
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return 127;
    }
    if (pid == 0) {
        close(fds[0]);
        if (chdir(cwd.c_str()) != 0) {
            writeStderr("Gagal masuk ke folder proyek.\n");
            _exit(127);
        }
        if (dup2(fds[1], STDOUT_FILENO) < 0 || dup2(fds[1], STDERR_FILENO) < 0) {
            writeStderr("Gagal menyiapkan keluaran perintah.\n");
            _exit(127);
        }
        close(fds[1]);
        execv(argv[0], argv.data());
        writeStderr("Gagal menjalankan perintah.\n");
        _exit(127);
    }

    close(fds[1]);
    char buffer[4096];
    for (;;) {
        ssize_t n = read(fds[0], buffer, sizeof(buffer));
        if (n > 0) {
            if (output) appendCapped(*output, buffer, static_cast<size_t>(n));
            continue;
        }
        if (n == 0) break;
        if (errno == EINTR) continue;
        break;
    }
    close(fds[0]);

    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) return 127;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 127;
}
#endif

}  // namespace

struct Updater::Impl {
    mutable std::mutex mutex;
    std::thread worker;
    std::atomic<bool> running{false};
    std::atomic<bool> finished{false};
    std::atomic<bool> ok{false};
    std::string phase = "Siap.";
    std::string error;
    std::string output;
    std::string restartPath;

    ~Impl() {
        if (worker.joinable()) worker.join();
    }

    void setPhase(const std::string& text) {
        std::lock_guard<std::mutex> lock(mutex);
        phase = text;
    }

    void append(const std::string& text) {
        if (text.empty()) return;
        std::lock_guard<std::mutex> lock(mutex);
        output += text;
        if (output.size() > kMaxOutput) output.erase(0, output.size() - kMaxOutput);
    }

    void complete(bool success, const std::string& message, const std::string& detail) {
        std::lock_guard<std::mutex> lock(mutex);
        phase = message;
        error = detail;
        ok.store(success);
        running.store(false);
        finished.store(true);
    }

    void run(std::string repo, std::string installPath) {
        try {
            runImpl(repo, installPath);
        } catch (const std::exception& e) {
            complete(false, "Pembaruan gagal.", std::string("Kesalahan: ") + e.what());
        } catch (...) {
            complete(false, "Pembaruan gagal.", "Kesalahan tidak dikenal.");
        }
    }

    void runImpl(const std::string& repo, const std::string& installPath) {
        std::error_code ec;
        const fs::path repoPath(repo);
        const fs::path binary = repoPath / (std::string("rpgcpp") + kExeSuffix);
        const fs::path backup = repoPath / (std::string("rpgcpp.old") + kExeSuffix);

        // Are we rebuilding the very binary that is running? Compare before
        // the old binary is renamed away.
        bool sameBinary = installPath.empty();
        if (!sameBinary) {
            std::error_code eqEc;
            if (fs::exists(installPath, eqEc)) {
                sameBinary = fs::equivalent(binary, installPath, eqEc);
            }
            if (!sameBinary) {
                std::error_code aEc;
                std::error_code bEc;
                sameBinary = fs::weakly_canonical(binary, aEc) ==
                                 fs::weakly_canonical(installPath, bEc) &&
                             !aEc && !bEc;
            }
        }

        // A previous update may have been interrupted after moving the binary.
        if (fs::exists(backup, ec)) {
            if (!fs::exists(binary, ec)) {
                fs::rename(backup, binary, ec);
            } else {
                fs::remove(backup, ec);
            }
            ec.clear();
        }

        setPhase("Memeriksa repositori...");
        std::string captured;
        int rc = runCommand(repo, {"git", "status", "--porcelain", "--untracked-files=no"},
                            &captured);
        append("\n$ git status --porcelain --untracked-files=no\n" + captured);
        if (rc != 0) {
            complete(false, "Pembaruan gagal.",
                     "Git tidak tersedia atau repo tidak valid.");
            return;
        }
        if (!trimCopy(captured).empty()) {
            complete(false, "Pembaruan gagal.",
                     "Ada perubahan lokal. Commit dulu sebelum update.");
            return;
        }

        setPhase("Menarik pembaruan dari Git...");
        captured.clear();
        rc = runCommand(repo, {"git", "pull", "--ff-only"}, &captured);
        append("\n$ git pull --ff-only\n" + captured);
        if (rc != 0) {
            complete(false, "Pembaruan gagal.",
                     "Git pull gagal. Periksa koneksi atau konflik.");
            return;
        }

        // Linux keeps the running inode alive after a rename, so the build can
        // replace the binary even while the old version is still executing.
        bool moved = false;
        if (fs::exists(binary, ec)) {
            fs::remove(backup, ec);
            ec.clear();
            fs::rename(binary, backup, ec);
            if (ec) {
                complete(false, "Pembaruan gagal.",
                         "Tidak bisa menyiapkan biner lama.");
                return;
            }
            moved = true;
        }

        auto restore = [&]() {
            std::error_code restoreEc;
            if (fs::exists(binary, restoreEc)) fs::remove(binary, restoreEc);
            if (moved && fs::exists(backup, restoreEc)) {
                fs::rename(backup, binary, restoreEc);
            }
        };

        unsigned jobs = std::thread::hardware_concurrency();
        if (jobs == 0) jobs = 2;
        jobs = std::min(jobs, 8u);

        // Match install.sh: honour CXX, otherwise pick whichever supported
        // compiler is actually available instead of assuming g++.
        std::string compiler;
        if (const char* env = std::getenv("CXX"); env && *env) {
            compiler = env;
        } else if (!findExecutablePath("g++").empty()) {
            compiler = "g++";
        } else if (!findExecutablePath("clang++").empty()) {
            compiler = "clang++";
        }

        std::string makeProgram = "make";
#ifdef _WIN32
        // MSYS2 ships mingw32-make under the MinGW toolchain; both work when
        // sh.exe is on PATH, so use whichever name exists.
        if (findExecutablePath("make").empty() &&
            !findExecutablePath("mingw32-make").empty()) {
            makeProgram = "mingw32-make";
        }
#endif
        std::vector<std::string> makeArgs{makeProgram, "-j" + std::to_string(jobs)};
        std::string makeLabel = "$ " + makeProgram + " -j" + std::to_string(jobs);
        if (!compiler.empty()) {
            makeArgs.push_back("CXX=" + compiler);
            makeLabel += " CXX=" + compiler;
        }

        setPhase("Membangun ulang game...");
        captured.clear();
        rc = runCommand(repo, makeArgs, &captured);
        append("\n" + makeLabel + "\n" + captured);
        if (rc != 0) {
            restore();
            complete(false, "Build gagal. Biner lama dipulihkan.",
                     "Build gagal. Biner lama dipulihkan.");
            return;
        }
        if (!fs::is_regular_file(binary, ec)) {
            restore();
            complete(false, "Build gagal.", "Make tidak menghasilkan biner.");
            return;
        }

        setPhase("Menguji biner baru...");
        captured.clear();
        rc = runCommand(repo, {binary.string(), "--headless", "--frames", "30"}, &captured);
        append("\n$ rpgcpp --headless --frames 30\n" + captured);
        if (rc != 0) {
            restore();
            complete(false, "Uji biner gagal. Biner lama dipulihkan.",
                     "Biner baru gagal diuji. Biner lama dipulihkan.");
            return;
        }

        std::string finalPath = binary.string();
        if (!sameBinary && !installPath.empty()) {
            setPhase("Memasang biner baru...");
            fs::path target(installPath);
            fs::path temporary = target;
            temporary += ".new";
            fs::copy_file(binary, temporary, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                restore();
                complete(false, "Gagal memasang biner baru.",
                         "Gagal memasang biner baru. Jalankan install.sh.");
                return;
            }
#ifndef _WIN32
            fs::permissions(temporary,
                            fs::perms::owner_all | fs::perms::group_read |
                                fs::perms::group_exec | fs::perms::others_read |
                                fs::perms::others_exec,
                            fs::perm_options::replace, ec);
            if (ec) {
                std::error_code removeEc;
                fs::remove(temporary, removeEc);
                restore();
                complete(false, "Gagal memasang biner baru.",
                         "Gagal mengatur izin biner baru.");
                return;
            }
#endif
#ifdef _WIN32
            // Windows keeps a running executable locked, but it can be
            // renamed. Move the old copy aside, put the new one in place, then
            // delete the leftover on the next launch.
            fs::path stale = target;
            stale += ".old";
            std::error_code staleEc;
            fs::remove(stale, staleEc);
            bool movedAside = false;
            if (fs::exists(target, ec)) {
                ec.clear();
                fs::rename(target, stale, ec);
                if (ec) {
                    std::error_code removeEc;
                    fs::remove(temporary, removeEc);
                    restore();
                    complete(false, "Gagal memasang biner baru.",
                             "Gagal mengganti biner lama. Jalankan install.sh.");
                    return;
                }
                movedAside = true;
            }
            ec.clear();
            fs::rename(temporary, target, ec);
            if (ec && movedAside) {
                std::error_code undoEc;
                fs::rename(stale, target, undoEc);
            }
#else
            fs::rename(temporary, target, ec);
#endif
            if (ec) {
                std::error_code removeEc;
                fs::remove(temporary, removeEc);
                restore();
                complete(false, "Gagal memasang biner baru.",
                         "Gagal mengganti biner lama. Jalankan install.sh.");
                return;
            }
            finalPath = target.string();
        }

        if (moved) {
            std::error_code removeEc;
            fs::remove(backup, removeEc);
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            restartPath = finalPath;
        }
        complete(true, "Pembaruan selesai. Z: mulai ulang.", "");
    }
};

Updater::Updater() : impl_(new Impl()) {}

Updater::~Updater() = default;

bool Updater::start(std::string* err) {
    std::string repo = findSourceCheckout();
    if (repo.empty()) {
        if (err) {
            *err = "Folder proyek tidak ditemukan. Jalankan dari folder sumber.";
        }
        return false;
    }
    return start(repo, currentExecutablePath(), err);
}

bool Updater::start(const std::string& repoDir, const std::string& installPath,
                    std::string* err) {
    if (impl_->running.load()) {
        if (err) *err = "Pembaruan sedang berjalan.";
        return false;
    }
    if (!isSourceCheckout(repoDir)) {
        if (err) *err = "Folder proyek tidak valid atau bukan checkout Git.";
        return false;
    }
    if (impl_->worker.joinable()) impl_->worker.join();

#ifdef _WIN32
    // The previous update may have left the replaced executable behind.
    if (!installPath.empty()) {
        std::error_code staleEc;
        fs::path stale(installPath);
        stale += ".old";
        fs::remove(stale, staleEc);
    }
#endif

    // A background update must never block on a credential prompt.
#ifdef _WIN32
    _putenv_s("GIT_TERMINAL_PROMPT", "0");
#else
    setenv("GIT_TERMINAL_PROMPT", "0", 1);
#endif

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->phase = "Memeriksa pembaruan...";
        impl_->error.clear();
        impl_->output.clear();
        impl_->restartPath.clear();
        impl_->ok.store(false);
        impl_->running.store(true);
        impl_->finished.store(false);
    }

    try {
        impl_->worker = std::thread(&Impl::run, impl_.get(), repoDir, installPath);
    } catch (const std::exception& e) {
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            impl_->phase = "Pembaruan gagal.";
            impl_->error = std::string("Gagal memulai pembaruan: ") + e.what();
            impl_->running.store(false);
            impl_->finished.store(true);
        }
        if (err) *err = impl_->error;
        return false;
    }

    if (err) err->clear();
    return true;
}

bool Updater::running() const { return impl_->running.load(); }

bool Updater::finished() const { return impl_->finished.load(); }

bool Updater::success() const { return impl_->ok.load(); }

std::string Updater::statusText() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->phase;
}

std::string Updater::errorText() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->error;
}

std::string Updater::output() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->output;
}

std::string Updater::restartPath() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->restartPath;
}

bool isSourceCheckout(const std::string& dir) {
    if (dir.empty()) return false;
    std::error_code ec;
    const fs::path root(dir);
    return fs::is_regular_file(root / "Makefile", ec) &&
           fs::is_regular_file(root / "src" / "main.cpp", ec) &&
           fs::exists(root / ".git", ec);
}

std::string executableDirectory() {
    std::string self = currentExecutablePath();
    if (self.empty()) return {};
    return fs::path(self).parent_path().string();
}

std::string defaultSavePath() {
    std::string dir = executableDirectory();
    if (!dir.empty()) {
        std::error_code ec;
        if (fs::exists(fs::path(dir) / "source", ec)) {
            return (fs::path(dir) / "savegame.dat").string();
        }
    }
    return "savegame.dat";
}

std::string findSourceCheckout() {
    std::vector<std::string> candidates;
    if (const char* env = std::getenv("AETHERIA_REPO"); env && *env) {
        candidates.emplace_back(env);
    }

    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    if (!ec) candidates.push_back(cwd.string());

    std::string self = currentExecutablePath();
    if (!self.empty()) {
        fs::path exeDir = fs::path(self).parent_path();
        candidates.push_back(exeDir.string());
        // Installed layout: install.sh clones the source beside the binary.
        candidates.push_back((exeDir / "source").string());
        // Older layout: $PREFIX/share/aetheria/source.
        candidates.push_back((exeDir.parent_path() / "share" / "aetheria" / "source").string());
    }

    if (const char* home = std::getenv("HOME"); home && *home) {
        candidates.push_back(
            (fs::path(home) / ".local" / "share" / "aetheria" / "source").string());
    }
    if (const char* data = std::getenv("XDG_DATA_HOME"); data && *data) {
        candidates.push_back((fs::path(data) / "aetheria" / "source").string());
    }
#ifdef _WIN32
    if (const char* profile = std::getenv("USERPROFILE"); profile && *profile) {
        candidates.push_back(
            (fs::path(profile) / ".aetheria" / "source").string());
    }
    if (const char* local = std::getenv("LOCALAPPDATA"); local && *local) {
        candidates.push_back((fs::path(local) / "Pelita" / "source").string());
        candidates.push_back((fs::path(local) / "Aetheria" / "source").string());
    }
#endif

    for (const std::string& candidate : candidates) {
        if (candidate.empty()) continue;
        fs::path path(candidate);
        std::error_code checkEc;
        if (fs::is_regular_file(path, checkEc)) {
            std::string pointed = readFirstLine(path);
            if (pointed.empty()) continue;
            path = pointed;
        }
        if (!path.is_absolute()) path = cwd / path;
        std::string dir = path.lexically_normal().string();
        if (isSourceCheckout(dir)) return dir;
    }
    return {};
}

}  // namespace rpg
