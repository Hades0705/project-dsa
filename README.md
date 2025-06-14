# project-dsa
File System Manager
This C++ application provides a command-line interface for navigating, managing, and interacting with your file system. It constructs an in-memory tree representation of a given directory and allows users to perform various operations like listing, creating, deleting, renaming, and searching files and directories.
Features
 * Hierarchical Display: Visualizes the file system structure as a tree.
 * Detailed View: Shows file sizes and last modified times.
 * File and Directory Management:
   * Create new folders.
   * Create new files.
   * Import (copy) existing files into the managed tree.
   * Rename files and folders.
   * Delete files and folders (with confirmation).
 * File Operations:
   * Open files with their default application.
   * View content of common text-based files directly within the application.
 * Search Functionality: Search for files and folders using regular expressions.
 * Cross-Platform Compatibility: Supports Windows, macOS, and Linux.
Getting Started
Prerequisites
To compile and run this application, you need:
 * A C++17 compatible compiler (e.g., GCC, Clang, MSVC).
 * std::filesystem support (included in C++17).
Compilation
Navigate to the directory containing the source code (.cpp file) in your terminal and compile it using your C++17 enabled compiler.
Example using g++:
g++ -std=c++17 -o file_manager main.cpp

 * -std=c++17: Specifies the C++17 standard, which is required for std::filesystem.
 * -o file_manager: Names the executable file_manager (you can choose a different name).
 * main.cpp: Replace with the actual name of your source file.
Running the Application
After successful compilation, you can run the executable from your terminal:
On Windows:
.\file_manager.exe

On macOS/Linux:
./file_manager

The application will start by building a file tree of the directory from which it was launched.
How to Use
Upon running the application, you'll see a menu of options:
=== FILE SYSTEM MANAGER ===
1. Display File Tree
2. Detailed File Tree View
3. Add New Folder
4. Add New File
5. Import Existing File
6. Open/View File
7. Rename File/Folder
8. Delete File/Folder
9. Search Files
10. Refresh Tree
11. Exit
Enter your choice (1-11):

Enter the number corresponding to your desired action and press Enter. Follow the on-screen prompts for each operation.
Important Notes
 * Ambiguity in findNode: The findNode function currently searches for the first occurrence of a file/folder by name. If multiple items have the same name within different directories, it will only interact with the first one it finds. For precise operations on specific items, you might need to ensure unique naming or enhance the findNode logic (e.g., by providing a full path).
 * Root Directory: The application operates on a tree built from its starting directory. Renaming or deleting the root directory from within the application's menu is not directly supported, as it represents the current working directory of the program itself.
 * File Content Display: For text files, only the first 100 lines are displayed by default. You can press Enter to view the next 100 lines or 'q' to quit viewing.
 * Error Handling: The application includes basic error handling for file system operations, but users should be cautious when performing deletion or modification actions.
