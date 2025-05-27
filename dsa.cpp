#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>          // Required for std::ifstream
#include <limits>
#include <system_error>

// For system() call (platform-dependent launching)
#ifdef _WIN32
#include <windows.h> // For ShellExecute if using that alternative, or just system()
#else
#include <cstdlib> // For system()
#endif

// Forward declaration
class FileSystemTree;

class Node {
public:
    enum Type { FILE, DIRECTORY };

    std::string name;
    Type type;
    std::filesystem::path fullPath;
    std::vector<Node*> children;

    Node(const std::string& name, const std::filesystem::path& path)
        : name(name), type(FILE), fullPath(path) {}

    Node(const std::string& name, const std::filesystem::path& path, Type t)
        : name(name), type(t), fullPath(path) {}

    ~Node() {
        for (Node* child : children) {
            delete child;
        }
    }

    void addChild(Node* child) {
        if (type == DIRECTORY) {
            children.push_back(child);
        } else {
            std::cerr << "Error: Cannot add children to a file node." << std::endl;
        }
    }

    void print(int indent = 0) const {
        for (int i = 0; i < indent; ++i) {
            std::cout << "  ";
        }
        std::cout << (type == DIRECTORY ? "📁 " : "📄 ") << name << std::endl;
        for (const auto& child : children) {
            child->print(indent + 1);
        }
    }
};

class FileSystemTree {
public:
    Node* root;

    FileSystemTree() : root(nullptr) {}

    ~FileSystemTree() {
        if (root) {
            delete root;
        }
    }

    Node* buildTree(const std::filesystem::path& currentPath) {
        if (!std::filesystem::exists(currentPath)) {
            std::cerr << "Error: Path does not exist: " << currentPath << std::endl;
            return nullptr;
        }

        Node* currentNode;
        if (std::filesystem::is_directory(currentPath)) {
            currentNode = new Node(currentPath.filename().string(), currentPath, Node::DIRECTORY);
            for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                Node* childNode = buildTree(entry.path());
                if (childNode) {
                    currentNode->addChild(childNode);
                }
            }
        } else if (std::filesystem::is_regular_file(currentPath)) {
            currentNode = new Node(currentPath.filename().string(), currentPath);
        } else {
            currentNode = new Node(currentPath.filename().string(), currentPath);
        }
        return currentNode;
    }

    void displayTree() const {
        if (root) {
            root->print();
        } else {
            std::cout << "Tree is empty." << std::endl;
        }
    }

    Node* findNode(Node* current, const std::string& targetName) {
        if (!current) {
            return nullptr;
        }
        if (current->name == targetName) {
            return current;
        }
        for (Node* child : current->children) {
            Node* found = findNode(child, targetName);
            if (found) {
                return found;
            }
        }
        return nullptr;
    }

    Node* findParent(Node* current, Node* targetChild) {
        if (!current || current->type != Node::DIRECTORY) {
            return nullptr;
        }
        for (Node* child : current->children) {
            if (child == targetChild) {
                return current;
            }
            if (child->type == Node::DIRECTORY) {
                Node* found = findParent(child, targetChild);
                if (found) return found;
            }
        }
        return nullptr;
    }

    bool deleteNode(Node* parent, Node* targetNode) {
        if (!parent || !targetNode) {
            std::cerr << "Error: Parent or target node is null for deletion." << std::endl;
            return false;
        }

        std::error_code ec;
        if (targetNode->type == Node::DIRECTORY) {
            if (std::filesystem::remove_all(targetNode->fullPath, ec)) {
                std::cout << "Successfully removed directory: " << targetNode->fullPath << std::endl;
            } else {
                std::cerr << "Error removing directory: " << targetNode->fullPath << " (" << ec.message() << ")" << std::endl;
                return false;
            }
        } else {
            if (std::filesystem::remove(targetNode->fullPath, ec)) {
                std::cout << "Successfully removed file: " << targetNode->fullPath << std::endl;
            } else {
                std::cerr << "Error removing file: " << targetNode->fullPath << " (" << ec.message() << ")" << std::endl;
                return false;
            }
        }

        for (size_t i = 0; i < parent->children.size(); ++i) {
            if (parent->children[i] == targetNode) {
                delete targetNode;
                parent->children.erase(parent->children.begin() + i);
                return true;
            }
        }
        std::cerr << "Error: Target node not found as a child of the specified parent." << std::endl;
        return false;
    }

    Node* createDirectory(Node* parent, const std::string& newFolderName) {
        if (!parent || parent->type != Node::DIRECTORY) {
            std::cerr << "Error: Cannot create directory in a non-directory parent or null parent." << std::endl;
            return nullptr;
        }

        std::filesystem::path newDirPath = parent->fullPath / newFolderName;
        std::error_code ec;
        if (std::filesystem::create_directory(newDirPath, ec)) {
            std::cout << "Successfully created directory: " << newDirPath << std::endl;
            Node* newNode = new Node(newFolderName, newDirPath, Node::DIRECTORY);
            parent->addChild(newNode);
            return newNode;
        } else {
            std::cerr << "Error creating directory: " << newDirPath << " (" << ec.message() << ")" << std::endl;
            return nullptr;
        }
    }

    Node* createFile(Node* parent, const std::string& newFileName) {
        if (!parent || parent->type != Node::DIRECTORY) {
            std::cerr << "Error: Cannot create file in a non-directory parent or null parent." << std::endl;
            return nullptr;
        }

        std::filesystem::path newFilePath = parent->fullPath / newFileName;
        std::ofstream ofs(newFilePath);
        if (ofs.is_open()) {
            ofs.close();
            std::cout << "Successfully created file: " << newFilePath << std::endl;
            Node* newNode = new Node(newFileName, newFilePath, Node::FILE);
            parent->addChild(newNode);
            return newNode;
        } else {
            std::cerr << "Error creating file: " << newFilePath << std::endl;
            return nullptr;
        }
    }

    bool renameNode(Node* targetNode, Node* newParent, const std::string& newName) {
        if (!targetNode || !newParent || newParent->type != Node::DIRECTORY) {
            std::cerr << "Error: Invalid target node or new parent for rename/move." << std::endl;
            return false;
        }

        std::filesystem::path newFullPath = newParent->fullPath / newName;
        std::error_code ec;

        std::filesystem::rename(targetNode->fullPath, newFullPath, ec);

        if (ec) {
            std::cerr << "Error renaming/moving '" << targetNode->fullPath << "' to '"
                      << newFullPath << "': " << ec.message() << std::endl;
            return false;
        }

        std::cout << "Successfully renamed/moved '" << targetNode->fullPath << "' to '"
                  << newFullPath << "'" << std::endl;

        Node* currentParent = findParent(root, targetNode);
        if (currentParent) {
            for (size_t i = 0; i < currentParent->children.size(); ++i) {
                if (currentParent->children[i] == targetNode) {
                    currentParent->children.erase(currentParent->children.begin() + i);
                    break;
                }
            }
        } else if (targetNode == root && targetNode->name != newName) {
            // Root renaming logic
        }

        targetNode->name = newName;
        targetNode->fullPath = newFullPath;

        if (newParent != currentParent || targetNode == root) {
             newParent->addChild(targetNode);
        }

        return true;
    }

    Node* importFile(Node* destinationParent, const std::filesystem::path& sourceFilePath) {
        if (!destinationParent || destinationParent->type != Node::DIRECTORY) {
            std::cerr << "Error: Destination must be a valid directory." << std::endl;
            return nullptr;
        }
        if (!std::filesystem::exists(sourceFilePath)) {
            std::cerr << "Error: Source file does not exist: " << sourceFilePath << std::endl;
            return nullptr;
        }
        if (!std::filesystem::is_regular_file(sourceFilePath)) {
            std::cerr << "Error: Source path is not a regular file: " << sourceFilePath << std::endl;
            return nullptr;
        }

        std::filesystem::path destinationFilePath = destinationParent->fullPath / sourceFilePath.filename();
        std::error_code ec;

        std::filesystem::copy(sourceFilePath, destinationFilePath, std::filesystem::copy_options::overwrite_existing, ec);

        if (ec) {
            std::cerr << "Error copying file from '" << sourceFilePath << "' to '"
                      << destinationFilePath << "': " << ec.message() << std::endl;
            return nullptr;
        }

        std::cout << "Successfully imported (copied) file to: " << destinationFilePath << std::endl;

        Node* importedNode = new Node(sourceFilePath.filename().string(), destinationFilePath, Node::FILE);
        destinationParent->addChild(importedNode);

        return importedNode;
    }

    /**
     * @brief Opens a file, either by displaying its text content or launching with the OS default application.
     * @param targetFileNode The Node representing the file to open.
     */
    void openFile(Node* targetFileNode) {
        if (!targetFileNode || targetFileNode->type != Node::FILE) {
            std::cerr << "Error: Cannot open a directory or a null node." << std::endl;
            return;
        }

        std::cout << "\n--- Opening: " << targetFileNode->fullPath << " ---" << std::endl;

        // Get the file extension
        std::string extension = targetFileNode->fullPath.extension().string();
        // Convert to lowercase for case-insensitive comparison
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        // Option 1: Display content for known text file types
        if (extension == ".txt" || extension == ".log" || extension == ".csv" ||
            extension == ".json" || extension == ".xml" || extension == ".cpp" ||
            extension == ".h" || extension == ".hpp") {
            
            std::ifstream file(targetFileNode->fullPath);
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    std::cout << line << std::endl;
                }
                file.close();
                std::cout << "\n--- End of file content ---" << std::endl;
            } else {
                std::cerr << "Error: Could not read text file content: " << targetFileNode->fullPath << std::endl;
            }
        } else {
            // Option 2: Launch with default OS application for other file types
            std::string command;
            std::string quotedPath = "\"" + targetFileNode->fullPath.string() + "\""; // Quote path for spaces

            #ifdef _WIN32
                // Windows: Use 'start' command. The first empty "" is for the window title.
                command = "start \"\" " + quotedPath;
            #elif __APPLE__
                // macOS: Use 'open' command
                command = "open " + quotedPath;
            #else
                // Linux: Use 'xdg-open' (common for many desktop environments)
                command = "xdg-open " + quotedPath;
            #endif

            // Execute the command
            int result = std::system(command.c_str());
            if (result != 0) {
                std::cerr << "Error: Failed to open file with OS default application. Command: " << command << std::endl;
            } else {
                std::cout << "Attempting to open file with OS default application..." << std::endl;
            }
        }
    }
};

// --- Helper functions for the user interface ---

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void displayMainMenu() {
    std::cout << "\n--- FILE/FOLDER MANAGEMENT PROGRAM ---" << std::endl;
    std::cout << "1. Display File Tree" << std::endl;
    std::cout << "2. Add New Folder" << std::endl;
    std::cout << "3. Add New File (Empty)" << std::endl;
    std::cout << "4. Import Existing File" << std::endl;
    std::cout << "5. Open File" << std::endl; // New option
    std::cout << "6. Rename File/Folder" << std::endl;
    std::cout << "7. Delete File/Folder" << std::endl;
    std::cout << "8. Exit" << std::endl; // Updated exit number
    std::cout << "Enter your choice (1-8): "; // Updated range
}

// --- Main function with interactive loop ---
int main() {
    std::filesystem::path startPath = ".";
    FileSystemTree fileTree;

    std::cout << "Initializing file tree from: " << startPath << std::endl;
    fileTree.root = fileTree.buildTree(startPath);
    if (!fileTree.root) {
        std::cerr << "Could not initialize file tree. Exiting program." << std::endl;
        return 1;
    }

    int choice;
    std::string name, parentName, newName, sourcePathStr;
    Node* selectedNode = nullptr;
    Node* parentNode = nullptr;

    do {
        fileTree.displayTree();
        displayMainMenu();
        std::cin >> choice;

        clearInputBuffer(); // Clear buffer after reading int choice

        switch (choice) {
            case 1: // Display File Tree
                break;
            case 2: // Add New Folder
                std::cout << "Enter parent folder name (leave blank for ROOT): ";
                std::getline(std::cin, parentName);

                parentNode = fileTree.root;
                if (!parentName.empty()) {
                    parentNode = fileTree.findNode(fileTree.root, parentName);
                }

                if (parentNode && parentNode->type == Node::DIRECTORY) {
                    std::cout << "Enter new folder name: ";
                    std::getline(std::cin, name);
                    fileTree.createDirectory(parentNode, name);
                } else {
                    std::cout << "Parent folder not found or is not a directory. Please try again." << std::endl;
                }
                break;
            case 3: // Add New File (Empty)
                std::cout << "Enter parent folder name (leave blank for ROOT): ";
                std::getline(std::cin, parentName);

                parentNode = fileTree.root;
                if (!parentName.empty()) {
                    parentNode = fileTree.findNode(fileTree.root, parentName);
                }

                if (parentNode && parentNode->type == Node::DIRECTORY) {
                    std::cout << "Enter new file name (e.g., my_doc.txt, image.jpg): ";
                    std::getline(std::cin, name);
                    fileTree.createFile(parentNode, name);
                } else {
                    std::cout << "Parent folder not found or is not a directory. Please try again." << std::endl;
                }
                break;
            case 4: // Import Existing File
                std::cout << "Enter the FULL path of the source file to import (e.g., C:\\Users\\User\\Documents\\my_file.docx): ";
                std::getline(std::cin, sourcePathStr);
                
                std::cout << "Enter the name of the DESTINATION folder in your tree (leave blank for ROOT): ";
                std::getline(std::cin, parentName);

                parentNode = fileTree.root;
                if (!parentName.empty()) {
                    parentNode = fileTree.findNode(fileTree.root, parentName);
                }

                if (parentNode && parentNode->type == Node::DIRECTORY) {
                    fileTree.importFile(parentNode, std::filesystem::path(sourcePathStr));
                } else {
                    std::cout << "Destination folder not found or is not a directory. Please try again." << std::endl;
                }
                break;
            case 5: // Open File (NEW)
                std::cout << "Enter the name of the file to open: ";
                std::getline(std::cin, name);

                selectedNode = fileTree.findNode(fileTree.root, name);
                if (selectedNode) {
                    fileTree.openFile(selectedNode);
                } else {
                    std::cout << "File with name '" << name << "' not found. Please try again." << std::endl;
                }
                break;
            case 6: // Rename File/Folder (Updated to 6)
                std::cout << "Enter the name of the file/folder to rename: ";
                std::getline(std::cin, name);

                selectedNode = fileTree.findNode(fileTree.root, name);
                if (selectedNode) {
                    std::cout << "Enter new name: ";
                    std::getline(std::cin, newName);
                    
                    Node* oldParent = fileTree.findParent(fileTree.root, selectedNode);
                    if (oldParent) {
                        fileTree.renameNode(selectedNode, oldParent, newName);
                    } else if (selectedNode == fileTree.root) {
                        fileTree.renameNode(selectedNode, selectedNode, newName);
                    } else {
                        std::cout << "Error: Could not find parent of node. Cannot rename." << std::endl;
                    }
                    
                } else {
                    std::cout << "File/folder with name '" << name << "' not found. Please try again." << std::endl;
                }
                break;
            case 7: // Delete File/Folder (Updated to 7)
                std::cout << "Enter the name of the file/folder to delete: ";
                std::getline(std::cin, name);

                selectedNode = fileTree.findNode(fileTree.root, name);
                if (selectedNode) {
                    if (selectedNode == fileTree.root) {
                        std::cout << "Warning: Deleting the root will delete ALL contents. Are you sure? (y/n): ";
                        char confirm;
                        std::cin >> confirm;
                        clearInputBuffer();
                        if (confirm == 'y' || confirm == 'Y') {
                            std::cout << "Root deletion is currently not supported for safety reasons." << std::endl;
                        } else {
                            std::cout << "Deletion cancelled." << std::endl;
                        }
                    } else {
                        parentNode = fileTree.findParent(fileTree.root, selectedNode);
                        if (parentNode) {
                            fileTree.deleteNode(parentNode, selectedNode);
                        } else {
                            std::cout << "Error: Could not find parent of node. Cannot delete." << std::endl;
                        }
                    }
                } else {
                    std::cout << "File/folder with name '" << name << "' not found. Please try again." << std::endl;
                }
                break;
            case 8: // Exit (Updated to 8)
                std::cout << "Exiting program. Goodbye!" << std::endl;
                break;
            default:
                std::cout << "Invalid choice. Please enter a number from 1 to 8." << std::endl;
                break;
        }
        std::cout << "\nPress Enter to continue...";
        std::cin.get();
        system("cls || clear");

    } while (choice != 8);

    return 0;
}
