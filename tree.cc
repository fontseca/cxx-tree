#include <string>
#include <filesystem>
#include <concepts>

/* File system namespace alias.  */
namespace fs = std::filesystem;

template <typename T>
/* Printable type.  */
concept Printable = std::integral<T> or std::floating_point<T> or std::is_pointer_v<T>;

/* The maximum number of sub-directories to
   recursively traverse.   */
static std::int32_t max_depth = 1U;

/* The total number of directories.  */
static std::size_t total_directories = 0UL;

/* The total number of files.  */
static std::size_t total_files = 0UL;

/* Emits the error `err' to stderr and quits the program.  */
[[noreturn]]
static void fatal(char const *const format, Printable auto const ...args) noexcept
{
  char fmt_buffer[0x512];
  std::sprintf(fmt_buffer, "cxx-tree: %s\n", format);
  std::fprintf(stderr, fmt_buffer, args...);
  std::exit(EXIT_FAILURE);
}

/* Counts the number of sub-directories inside `directory'.  */
[[nodiscard]]
static inline auto number_of_directories_in_directory(fs::path const &directory,
  std::error_code &err) noexcept
  -> std::size_t
{
  return std::count_if(fs::directory_iterator(directory, err), fs::directory_iterator { },
    [&err](std::filesystem::path const &path)
    {
      return fs::is_directory(path, err);
    }
  );
}

/* Counts the number of files inside `directory'.  */
[[nodiscard]]
static inline auto number_of_files_in_directory(fs::path const &directory,
  std::error_code &err) noexcept
  -> std::size_t
{
  return std::count_if(fs::directory_iterator(directory, err), fs::directory_iterator { },
    [&err](fs::path const &path)
    {
      return not fs::is_directory(path, err);
    }
  );
}

/* Walks the directory `directory' and its following `level' subdirectories.  */
static void traverse(fs::path const &directory, std::error_code &err,
  std::int32_t const level = 1, std::string segment = "") noexcept
{
  std::size_t const n_directories = ::number_of_directories_in_directory(directory, err);
  std::size_t const n_files = ::number_of_files_in_directory(directory, err);

  /* Counter of the number of entries in the current
     directory. */
  std::size_t entries_count { 0 };

  /* Augment statistics inside the directory.  */

  ::total_directories += n_directories;
  ::total_files += n_files;

  /* Traverse directory content.  */

  for (fs::directory_entry const &entry : fs::directory_iterator(directory, err))
  {
    bool const is_directory = fs::is_directory(entry, err);
    bool const is_symlink = fs::is_symlink(entry, err);

    /* Canonical path for symlinks.  */
    std::string const canonical_path = fs::is_symlink(entry, err) ? fs::canonical(entry, err) : "";

    /* The name of the current entry (file or directory).  */
    std::string const entry_name = entry.path().filename().c_str();

    if (is_directory)
    {
      /* Dummy operation to catch possibles errors
         (we want EACCES in this case) in the current
         directory.  */

      fs::directory_iterator(entry, err);
    }

    /* Positive when access to the current directory
       was not granted.  */
    bool const access_denied = EACCES == err.value() ? true : false;

    /* Format string.  */
    char const *const format =
    is_symlink
      ? is_directory
        ? "%s%s \x1B[94m%s\033[0m -> \x1B[92m%s\033[0m\n" /* Symlik (directory).  */
        : "%s%s %s -> \x1B[92m%s\033[0m\n"                /* Symlik (file).  */
      : is_directory
        ? access_denied and level != ::max_depth
          ? "%s%s \x1B[94m%s\033[0m [%s]\n"               /* Access was denied.  */
          : "%s%s \x1B[94m%s\033[0m\n"                    /* Normal directory.  */
        : "%s%s %s\n";                                    /* Normal file.  */

    char const *const left = ++entries_count == n_directories + n_files ? "└──" : "├──";
    char const *const right = entries_count == n_directories + n_files ? "    " : "│   ";

    is_symlink
      ? std::printf(format, segment.c_str(), left, entry_name.c_str(), canonical_path.c_str())
      : access_denied and level != ::max_depth
        ? std::printf(format, segment.c_str(), left, entry_name.c_str(), "access denied")
        : std::printf(format, segment.c_str(), left, entry_name.c_str());

    /* Recursively traverse the next directory in the
       current one.  */

    if (is_directory and not is_symlink and not access_denied and level != ::max_depth)
    {
      ::traverse(entry, err, 1 + level, segment + right);
    }
  }
}

auto main([[maybe_unused]] std::int32_t argc, [[maybe_unused]] char *argv[])
  -> std::int32_t
{
  /* Directory to traverse.  */
  fs::path const directory { argv[1] ? argv[1] : "." };

  std::error_code err { };

  if (not fs::exists(directory, err))
  {
    fatal("cannot access '%s': No such directory", directory.c_str());
  }
  else if (not fs::is_directory(directory, err))
  {
    fatal("%s: Not a directory", directory.c_str());
  }

  if (3 <= argc)
  {
    ::max_depth = std::atoi(argv[2]);
    ::max_depth = ::max_depth < 1 ? 1 : ::max_depth;
  }

  /* Base directory at top.  */

  std::printf("%s\n", directory.c_str());

  /* Start traversal.  */

  ::traverse(directory, err);

  /* Print statistics.  */
  std::printf("\n%lu %s, %lu %s\n",
    ::total_directories, ::total_directories == 1 ? "directory" : "directories",
      ::total_files, ::total_files == 1 ? "file" : "files");

  return EXIT_SUCCESS;
}
