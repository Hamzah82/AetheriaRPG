// updater.h - pull the source checkout and rebuild from inside the game
#pragma once
#include <memory>
#include <string>

namespace rpg {

// Runs "git pull --ff-only" and "make" on a background thread, then reports
// progress so the title screen can show what is happening. The running game
// keeps drawing while the update is in progress.
class Updater {
public:
    Updater();
    ~Updater();

    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;

    // Locate a source checkout and rebuild it. installPath is the currently
    // running executable; when it lives outside the checkout the freshly
    // built binary is copied there as well.
    bool start(std::string* err = nullptr);

    // Explicit variant, also useful for tests.
    bool start(const std::string& repoDir, const std::string& installPath,
               std::string* err = nullptr);

    bool running() const;
    bool finished() const;
    bool success() const;

    std::string statusText() const;
    std::string errorText() const;
    std::string output() const;

    // Executable to launch when restarting after a successful update. Empty
    // until an update has completed.
    std::string restartPath() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// True when dir contains a Makefile, src/main.cpp, and a .git entry.
bool isSourceCheckout(const std::string& dir);

// Search cwd, the executable directory, AETHERIA_REPO, and the marker written
// by install.sh. Returns an empty string when no checkout is found.
std::string findSourceCheckout();

// Directory containing the running executable, or empty when unknown.
std::string executableDirectory();

// Default save file. An installed copy (recognised by a "source" checkout next
// to the binary) keeps savegame.dat beside the binary; a source checkout keeps it in
// the current directory.
std::string defaultSavePath();

}  // namespace rpg
