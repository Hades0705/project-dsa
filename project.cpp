#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>
#include <algorithm>
#include <memory>
#include <regex>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace fs = std::filesystem;

// ==================== Improved Node Class ====================
class Node {
public:
    enum Type { FILE, DIRECTORY };

    std::string name;
    Type type;
    fs::path fullPath;
    std::vector<std::unique_ptr<Node>> children;
    time_t lastModified;
    uintmax_t size; // in bytes

    Node(const std::string& name, const fs::path& path, Type t = FILE)
        : name(name), type(t), fullPath(path) {
        updateFileInfo();
    }

    void updateFileInfo() {
        try {
            if (fs::exists(fullPath)) {
                // Correct way to convert fs::file_time_type to time_t
                auto ftime = fs::last_write_time(fullPath);
                
                // Cast to system_clock::time_point and then convert
                auto sctime = std::chrono::file_clock::to_sys(ftime);
                lastModified = std::chrono::system_clock::to_time_t(sctime);

                size = type == DIRECTORY ? 0 : fs::file_size(fullPath);
            } else {
                lastModified = 0; // Or some indicator for non-existent
                size = 0;
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error updating info for " << fullPath << ": " << e.what() << "\n";
            lastModified = 0;
            size = 0;
        } catch (const std::exception& e) {
            std::cerr << "General error updating info for " << fullPath << ": " << e.what() << "\n";
            lastModified = 0;
            size = 0;
        }
    }

    void addChild(std::unique_ptr<Node> child) {
        if (type == DIRECTORY) {
            children.push_back(std::move(child));
        } else {
            std::cerr << "Error: Cannot add children to a file node.\n";
        }
    }

    void print(int indent = 0, bool showDetails = false) const {
        std::cout << std::string(indent * 2, ' ')
                  << (type == DIRECTORY ? "📁 " : "📄 ")
                  << name;

        if (showDetails) {
            std::cout << "  " << formatSize(size) << "  "
                      << formatTime(lastModified);
        }

        std::cout << "\n";

        for (const auto& child : children) {
            child->print(indent + 1, showDetails);
        }
    }

    static std::string formatSize(uintmax_t bytes) {
        constexpr const char* sizes[] = {"B", "KB", "MB", "GB"};
        double size = bytes;
        int i = 0;
        while (size >= 1024 && i < 3) {
            size /= 1024;
            i++;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << sizes[i];
        return oss.str();
    }

    static std::string formatTime(time_t time) {
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        return buffer;
    }
};

// ==================== Enhanced FileSystemTree Class ====================
class FileSystemTree {
public:
    std::unique_ptr<Node> root;
    std::string currentSearchTerm;
    std::vector<Node*> searchResults;

    FileSystemTree() = default;

    std::unique_ptr<Node> buildTree(const fs::path& currentPath) {
        if (!fs::exists(currentPath)) {
            std::cerr << "Error: Path does not exist: " << currentPath << "\n";
            return nullptr;
        }

        std::unique_ptr<Node> currentNode;
        try {
            if (fs::is_directory(currentPath)) {
                currentNode = std::make_unique<Node>(
                    currentPath.filename().string(), currentPath, Node::DIRECTORY);

                // Show loading progress for large directories
                size_t itemCount = 0;
                auto startTime = std::chrono::steady_clock::now();

                for (const auto& entry : fs::directory_iterator(currentPath)) {
                    try {
                        auto childNode = buildTree(entry.path());
                        if (childNode) {
                            currentNode->addChild(std::move(childNode));
                            itemCount++;

                            // Show progress every 100 items
                            if (itemCount % 100 == 0) {
                                auto now = std::chrono::steady_clock::now();
                                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
                                std::cout << "\rLoading... " << itemCount << " items ("
                                          << elapsed << "ms)";
                                std::cout.flush();
                            }
                        }
                    } catch (...) {
                        continue; // Skip problematic entries
                    }
                }

                if (itemCount >= 100) {
                    std::cout << "\r" << std::string(50, ' ') << "\r"; // Clear line
                }
            } else {
                currentNode = std::make_unique<Node>(
                    currentPath.filename().string(), currentPath);
            }
        } catch (...) {
            std::cerr << "Error building tree for: " << currentPath << "\n";
            return nullptr;
        }

        return currentNode;
    }

    void displayTree(bool showDetails = false) const {
        if (root) {
            root->print(0, showDetails);
        } else {
            std::cout << "Tree is empty.\n";
        }
    }

    Node* findNode(Node* current, const std::string& targetName) {
        if (!current) return nullptr;
        if (current->name == targetName) return current;

        for (const auto& child : current->children) {
            Node* found = findNode(child.get(), targetName);
            if (found) return found;
        }
        return nullptr;
    }

    Node* findParent(Node* current, Node* targetChild) {
        if (!current || current->type != Node::DIRECTORY) return nullptr;

        for (const auto& child : current->children) {
            if (child.get() == targetChild) return current;
            if (child->type == Node::DIRECTORY) {
                Node* found = findParent(child.get(), targetChild);
                if (found) return found;
            }
        }
        return nullptr;
    }

    bool deleteNode(Node* parent, Node* targetNode) {
        if (!parent || !targetNode) {
            std::cerr << "Error: Parent or target node is null.\n";
            return false;
        }

        std::error_code ec;
        bool success = false;

        try {
            if (targetNode->type == Node::DIRECTORY) {
                success = fs::remove_all(targetNode->fullPath, ec) > 0;
            } else {
                success = fs::remove(targetNode->fullPath, ec);
            }

            if (success) {
                std::cout << "Successfully removed: " << targetNode->fullPath << "\n";
                // Remove from parent's children
                auto& children = parent->children;
                children.erase(std::remove_if(children.begin(), children.end(),
                    [targetNode](const std::unique_ptr<Node>& child) {
                        return child.get() == targetNode;
                    }), children.end());
                return true;
            }
        } catch (...) {
            ec = std::error_code(errno, std::generic_category());
        }

        std::cerr << "Error removing " << targetNode->fullPath
                  << ": " << ec.message() << "\n";
        return false;
    }

    Node* createDirectory(Node* parent, const std::string& newFolderName) {
        if (!parent || parent->type != Node::DIRECTORY) {
            std::cerr << "Error: Invalid parent directory.\n";
            return nullptr;
        }

        fs::path newDirPath = parent->fullPath / newFolderName;
        std::error_code ec;

        if (fs::create_directory(newDirPath, ec)) {
            std::cout << "Created directory: " << newDirPath << "\n";
            auto newNode = std::make_unique<Node>(newFolderName, newDirPath, Node::DIRECTORY);
            auto* rawPtr = newNode.get();
            parent->children.push_back(std::move(newNode));
            return rawPtr;
        }

        std::cerr << "Error creating directory: " << newDirPath
                  << ": " << ec.message() << "\n";
        return nullptr;
    }

    Node* createFile(Node* parent, const std::string& newFileName) {
        if (!parent || parent->type != Node::DIRECTORY) {
            std::cerr << "Error: Invalid parent directory.\n";
            return nullptr;
        }

        fs::path newFilePath = parent->fullPath / newFileName;

        try {
            std::ofstream ofs(newFilePath);
            if (ofs) {
                ofs.close();
                std::cout << "Created file: " << newFilePath << "\n";
                auto newNode = std::make_unique<Node>(newFileName, newFilePath);
                auto* rawPtr = newNode.get();
                parent->children.push_back(std::move(newNode));
                return rawPtr;
            }
        } catch (...) {
            std::error_code ec(errno, std::generic_category());
            std::cerr << "Error creating file: " << newFilePath
                      << ": " << ec.message() << "\n";
        }

        return nullptr;
    }

    bool renameNode(Node* targetNode, Node* newParent, const std::string& newName) {
        if (!targetNode || !newParent || newParent->type != Node::DIRECTORY) {
            std::cerr << "Error: Invalid nodes for rename operation.\n";
            return false;
        }

        fs::path newFullPath = newParent->fullPath / newName;
        std::error_code ec;

        try {
            fs::rename(targetNode->fullPath, newFullPath, ec);
            if (ec) throw std::runtime_error(ec.message());

            // Update the node's properties
            targetNode->name = newName;
            targetNode->fullPath = newFullPath;
            targetNode->updateFileInfo();

            // If moved to a different parent, update parent-child relationships
            Node* oldParent = findParent(root.get(), targetNode);
            if (oldParent && oldParent != newParent) {
                // Find and transfer the unique_ptr from old parent to new parent
                std::unique_ptr<Node> nodeToMove;
                auto& oldChildren = oldParent->children;
                for (auto it = oldChildren.begin(); it != oldChildren.end(); ++it) {
                    if (it->get() == targetNode) {
                        nodeToMove = std::move(*it);
                        oldChildren.erase(it);
                        break;
                    }
                }
                if (nodeToMove) {
                    newParent->children.push_back(std::move(nodeToMove));
                }
            }

            std::cout << "Successfully renamed/moved to: " << newFullPath << "\n";
            return true;
        } catch (...) {
            std::cerr << "Error renaming/moving: " << ec.message() << "\n";
            return false;
        }
    }

    Node* importFile(Node* destinationParent, const fs::path& sourceFilePath) {
        if (!destinationParent || destinationParent->type != Node::DIRECTORY) {
            std::cerr << "Error: Invalid destination directory.\n";
            return nullptr;
        }

        if (!fs::exists(sourceFilePath)) {
            std::cerr << "Error: Source file does not exist.\n";
            return nullptr;
        }

        fs::path destinationFilePath = destinationParent->fullPath / sourceFilePath.filename();
        std::error_code ec;

        try {
            fs::copy(sourceFilePath, destinationFilePath,
                    fs::copy_options::overwrite_existing, ec);
            if (ec) throw std::runtime_error(ec.message());

            auto newNode = std::make_unique<Node>(
                sourceFilePath.filename().string(), destinationFilePath);
            auto* rawPtr = newNode.get();
            destinationParent->children.push_back(std::move(newNode));

            std::cout << "Successfully imported: " << destinationFilePath << "\n";
            return rawPtr;
        } catch (...) {
            std::cerr << "Error importing file: " << ec.message() << "\n";
            return nullptr;
        }
    }

    void openFile(Node* targetFileNode) {
        if (!targetFileNode || targetFileNode->type != Node::FILE) {
            std::cerr << "Error: Invalid file node.\n";
            return;
        }

        std::cout << "\n--- Opening: " << targetFileNode->fullPath << " ---\n";

        std::string extension = targetFileNode->fullPath.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        // Text file extensions to display directly
        const std::vector<std::string> textExtensions = {
            ".txt", ".log", ".csv", ".json", ".xml",
            ".cpp", ".h", ".hpp", ".java", ".py", ".js", ".html", ".css"
        };

        if (std::find(textExtensions.begin(), textExtensions.end(), extension) != textExtensions.end()) {
            displayFileContent(targetFileNode->fullPath);
        } else {
            openWithDefaultApp(targetFileNode->fullPath);
        }
    }

    void searchFiles(const std::string& pattern, Node* current = nullptr) {
        if (!current) {
            current = root.get();
            searchResults.clear();
            currentSearchTerm = pattern;
        }

        if (!current) return;

        try {
            std::regex re(pattern, std::regex_constants::icase);
            if (std::regex_search(current->name, re)) {
                searchResults.push_back(current);
            }

            for (const auto& child : current->children) {
                searchFiles(pattern, child.get());
            }
        } catch (const std::regex_error& e) {
            std::cerr << "Invalid search pattern: " << e.what() << "\n";
        }
    }

    void displaySearchResults() const {
        if (searchResults.empty()) {
            std::cout << "No results found for: " << currentSearchTerm << "\n";
            return;
        }

        std::cout << "Search results (" << searchResults.size() << ") for: "
                  << currentSearchTerm << "\n";
        for (const auto& result : searchResults) {
            std::cout << "  " << (result->type == Node::DIRECTORY ? "📁 " : "📄 ")
                      << result->name << "  " << result->fullPath << "\n";
        }
    }

private:
    void displayFileContent(const fs::path& filePath) {
        try {
            std::ifstream file(filePath);
            if (!file) throw std::runtime_error("Could not open file");

            std::cout << "\n--- File Content ---\n";
            std::string line;
            int lineCount = 0;
            const int maxLines = 100;

            while (std::getline(file, line) && lineCount < maxLines) {
                std::cout << line << "\n";
                lineCount++;
            }

            if (file.eof()) {
                std::cout << "\n--- End of file ---\n";
            } else {
                std::cout << "\n--- First " << maxLines << " lines shown ---\n";
                std::cout << "Press Enter to continue or 'q' to quit...";
                char c;
                // Loop to consume characters until newline or 'q'
                while (std::cin.get(c) && c != 'q' && c != '\n') {
                    // Continue reading if character is not 'q' or newline
                }
                // If 'q' was pressed, break out of loop
                if (c == 'q') {
                    std::cout << "\n"; // Newline after 'q'
                    return;
                }
                // If Enter was pressed, continue displaying
                while (std::getline(file, line) && lineCount++ < maxLines * 2) { // Show next 100 lines
                    std::cout << line << "\n";
                }
                if (file.eof()) {
                    std::cout << "\n--- End of file ---\n";
                } else {
                    std::cout << "\n--- Next " << maxLines << " lines shown ---\n";
                }
            }
        } catch (...) {
            std::cerr << "Error reading file content.\n";
        }
    }

    void openWithDefaultApp(const fs::path& filePath) {
        std::string command;
        std::string quotedPath = "\"" + filePath.string() + "\"";

#ifdef _WIN32
        command = "start \"\" " + quotedPath;
#elif __APPLE__
        command = "open " + quotedPath;
#else
        command = "xdg-open " + quotedPath;
#endif

        int result = std::system(command.c_str());
        if (result != 0) {
            std::cerr << "Failed to open file with default application.\n";
        }
    }
};

// ==================== Enhanced User Interface ====================
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void displayMainMenu() {
    std::cout << "\n=== FILE SYSTEM MANAGER ===\n";
    std::cout << "1. Display File Tree\n";
    std::cout << "2. Detailed File Tree View\n";
    std::cout << "3. Add New Folder\n";
    std::cout << "4. Add New File\n";
    std::cout << "5. Import Existing File\n";
    std::cout << "6. Open/View File\n";
    std::cout << "7. Rename File/Folder\n";
    std::cout << "8. Delete File/Folder\n";
    std::cout << "9. Search Files\n";
    std::cout << "10. Refresh Tree\n";
    std::cout << "11. Exit\n";
    std::cout << "Enter your choice (1-11): ";
}

void pressEnterToContinue() {
    std::cout << "\nPress Enter to continue...";
    // Clear any leftover characters including newline from previous input
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get(); // Wait for user to press Enter
}

int main() {
    FileSystemTree fileTree;
    fs::path startPath = fs::current_path();

    std::cout << "Initializing file tree from: " << startPath << "\n";
    fileTree.root = fileTree.buildTree(startPath);
    if (!fileTree.root) {
        std::cerr << "Failed to initialize file tree.\n";
        return 1;
    }

    int choice;
    std::string input, name, parentName, newName, sourcePathStr;

    do {
        clearScreen();
        // Always display the tree first for context
        fileTree.displayTree();
        displayMainMenu();

        // Use a temporary string to read the whole line for choice to handle potential
        // extra characters and then convert to int. This makes std::cin.ignore() cleaner.
        std::string choiceStr;
        std::getline(std::cin, choiceStr);
        try {
            choice = std::stoi(choiceStr);
        } catch (const std::invalid_argument& e) {
            choice = 0; // Invalid choice
        } catch (const std::out_of_range& e) {
            choice = 0; // Invalid choice (too large/small)
        }


        Node* selectedNode = nullptr;
        Node* parentNode = nullptr;

        switch (choice) {
            case 1: // Display basic tree (already done at start of loop)
                pressEnterToContinue();
                break;
            case 2: // Detailed view
                clearScreen(); // Clear again to show only detailed tree
                fileTree.displayTree(true);
                pressEnterToContinue();
                break;
            case 3: { // Add folder
                std::cout << "Parent folder (blank for current directory): ";
                std::getline(std::cin, parentName);
                parentNode = parentName.empty() ? fileTree.root.get() :
                    fileTree.findNode(fileTree.root.get(), parentName); // findNode is still problematic for ambiguity

                if (parentNode && parentNode->type == Node::DIRECTORY) {
                    std::cout << "New folder name: ";
                    std::getline(std::cin, name);
                    if (!name.empty()) { // Basic validation
                         fileTree.createDirectory(parentNode, name);
                    } else {
                        std::cout << "Folder name cannot be empty.\n";
                    }
                } else {
                    std::cout << "Invalid or non-existent parent directory.\n";
                }
                pressEnterToContinue();
                break;
            }
            case 4: { // Add file
                std::cout << "Parent folder (blank for current directory): ";
                std::getline(std::cin, parentName);
                parentNode = parentName.empty() ? fileTree.root.get() :
                    fileTree.findNode(fileTree.root.get(), parentName);

                if (parentNode && parentNode->type == Node::DIRECTORY) {
                    std::cout << "New file name: ";
                    std::getline(std::cin, name);
                    if (!name.empty()) { // Basic validation
                        fileTree.createFile(parentNode, name);
                    } else {
                        std::cout << "File name cannot be empty.\n";
                    }
                } else {
                    std::cout << "Invalid or non-existent parent directory.\n";
                }
                pressEnterToContinue();
                break;
            }
            case 5: { // Import file
                std::cout << "Source file path: ";
                std::getline(std::cin, sourcePathStr);
                std::cout << "Destination folder (blank for current directory): ";
                std::getline(std::cin, parentName);
                parentNode = parentName.empty() ? fileTree.root.get() :
                    fileTree.findNode(fileTree.root.get(), parentName);

                if (parentNode && parentNode->type == Node::DIRECTORY) {
                    fileTree.importFile(parentNode, fs::path(sourcePathStr));
                } else {
                    std::cout << "Invalid or non-existent destination directory.\n";
                }
                pressEnterToContinue();
                break;
            }
            case 6: { // Open file
                std::cout << "File name to open: ";
                std::getline(std::cin, name);
                selectedNode = fileTree.findNode(fileTree.root.get(), name); // Still has ambiguity
                if (selectedNode) {
                    fileTree.openFile(selectedNode);
                } else {
                    std::cout << "File not found. (Note: if multiple files have this name, only the first found is considered.)\n";
                }
                pressEnterToContinue();
                break;
            }
            case 7: { // Rename
                std::cout << "Item to rename: ";
                std::getline(std::cin, name);
                selectedNode = fileTree.findNode(fileTree.root.get(), name); // Still has ambiguity
                if (selectedNode) {
                    std::cout << "New name: ";
                    std::getline(std::cin, newName);
                    if (!newName.empty()) {
                        Node* parent = fileTree.findParent(fileTree.root.get(), selectedNode);
                        if (parent) { // Cannot rename root using this mechanism easily without changing `findParent` logic or having a direct root check
                            fileTree.renameNode(selectedNode, parent, newName); // Assumes newParent is same as oldParent for rename
                        } else if (selectedNode == fileTree.root.get()) {
                             std::cout << "Renaming the root directory is not supported via this menu (it corresponds to the program's starting directory).\n";
                        } else {
                            std::cout << "Error finding parent of the item.\n";
                        }
                    } else {
                        std::cout << "New name cannot be empty.\n";
                    }
                } else {
                    std::cout << "Item not found. (Note: if multiple items have this name, only the first found is considered.)\n";
                }
                pressEnterToContinue();
                break;
            }
            case 8: { // Delete
                std::cout << "Item to delete: ";
                std::getline(std::cin, name);
                selectedNode = fileTree.findNode(fileTree.root.get(), name); // Still has ambiguity
                if (selectedNode) {
                    if (selectedNode == fileTree.root.get()) {
                        std::cout << "Cannot delete root directory.\n";
                    } else {
                        Node* parent = fileTree.findParent(fileTree.root.get(), selectedNode);
                        if (parent) {
                            std::cout << "Confirm delete '" << name << "'? (y/n): ";
                            char confirm;
                            std::cin >> confirm;
                            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Clear buffer after char read
                            if (confirm == 'y' || confirm == 'Y') {
                                fileTree.deleteNode(parent, selectedNode);
                            } else {
                                std::cout << "Deletion cancelled.\n";
                            }
                        } else {
                            std::cout << "Error finding parent to delete.\n";
                        }
                    }
                } else {
                    std::cout << "Item not found. (Note: if multiple items have this name, only the first found is considered.)\n";
                }
                pressEnterToContinue();
                break;
            }
            case 9: { // Search
                std::cout << "Search pattern (regex): ";
                std::getline(std::cin, name);
                fileTree.searchFiles(name);
                fileTree.displaySearchResults();
                pressEnterToContinue();
                break;
            }
            case 10: // Refresh
                fileTree.root = fileTree.buildTree(startPath);
                std::cout << "File tree refreshed.\n";
                pressEnterToContinue();
                break;
            case 11: // Exit
                std::cout << "Exiting...\n";
                break;
            default:
                std::cout << "Invalid choice. Please enter a number between 1 and 11.\n";
                pressEnterToContinue();
                break;
        }
    } while (choice != 11);

    return 0;
}
