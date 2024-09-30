#include <filesystem>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdio>

constexpr char LOG_NAME[] = "log";

class LogWriter {
public:
    LogWriter(const std::filesystem::path& directory) {
        const auto& filename = directory / LOG_NAME;
        log_.open(filename, std::ios_base::app);
    }

    void Append(const std::string& line) {
        log_ << line;
        log_.flush();
    }

    ~LogWriter() {
        log_.close();
    }

private:
    std::ofstream log_;
};

class LogReader {
public:
    LogReader(const std::filesystem::path& directory) {
        filename_ = directory / LOG_NAME;
    }

    void Start() {
        log_.open(filename_);
    }

    bool Next(char& type, std::string& content, int& index) {
        if (!log_) {
            return false;
        }
        if (log_.peek() == std::ifstream::traits_type::eof()) {
            return false;
        }

        log_.get(type);
        if (type == '0') {
            readEraseLine(index);
        } else {
            readPushLine(content);
        }

        return true;
    }

    void Close() {
        log_.close();
    }

private:
    void readEraseLine(int& index) {
        readCommaFromInput();

        index = 0;
        char c;
        log_.get(c);
        while (c != '\n') {
            index = index * 10 + charToInt(c);
            log_.get(c);
        }
    }

    void readPushLine(std::string& content) {
        readCommaFromInput();

        char s[10];
        char c;
        log_.get(s, 10, ',');
        int n = atoi(s);

        readCommaFromInput();

        content.clear();
        content.resize(n);
        for (int i = 0; i < n; i++) {
            log_.get(c);
            content[i] = c;
        }

        readNewLineFromInput();
    }

    int charToInt(char c) {
        return c - '0';
    }

    void readCommaFromInput() {
        skipChar();
    }
    void readNewLineFromInput() {
        skipChar();
    }
    void skipChar() {
        char c;
        log_.get(c);
    }

    std::ifstream log_;
    std::string filename_;
};

/**
 * Persistent vector implementation.
 */
class PersistentVector {
public:
    /**
     * Create a new vector that can be persistent to `directory`.
     */
    PersistentVector(const std::filesystem::path& directory) : log_writer_(directory) {
        LogReader lr(directory);
        lr.Start();
        std::string content;
        int index;
        char type;
        while(lr.Next(type, content, index)) {
            if (type == '0') {
                data_.erase(data_.begin() + index);
            } else {
                data_.push_back(content);
            }
        }
        lr.Close();
    }

    void push_back(const std::string& v) {
        log_writer_.Append(createPushLog(v));
        data_.push_back(v);
    }

    std::string_view at(std::size_t index) const {
        return data_[index];
    }

    void erase(std::size_t index) {
        log_writer_.Append(createEraseLog(index));
        data_.erase(data_.begin() + index);
    }

    std::size_t size() const {
        return data_.size();
    }

private:
    std::string createPushLog(const std::string& v) {
        char buffer[10];
        snprintf(buffer, 10, "%lu", v.size());
        std::string line("1,");
        line.append(buffer);
        line.append(",");
        line.append(v);
        line.push_back('\n');
        return line;
    }

    std::string createEraseLog(unsigned index) {
        char idx_str[10];
        snprintf(idx_str, 10, "%u", index);
        std::string line("0,");
        line.append(idx_str);
        line.push_back('\n');
        return line;
    }

    std::vector<std::string> data_;
    LogWriter log_writer_;
};

std::size_t errors = 0;

#define ERROR(msg) { \
    std::cout << __FILE__ << ":" << __LINE__ << " " << msg << "\n"; \
    ++errors; \
}
#define CHECK(x) do if (!(x)) { ERROR(#x " failed"); } while (false)

constexpr unsigned LOOP_COUNT = 100000;

std::string all_chars() {
    std::string rv;
    for (char c = std::numeric_limits<char>::min(); c != std::numeric_limits<char>::max(); ++c)
    {
        rv += c;
    }

    rv += std::numeric_limits<char>::max();
    return rv;
}

void run_test_one(const std::filesystem::path& p) {
    PersistentVector v(p);
    using namespace std::literals;

    v.push_back("foo");
    CHECK(v.at(0) == "foo");
    CHECK(v.size() == 1);

    v.push_back(all_chars());
    CHECK(v.at(1) == all_chars());
    CHECK(v.size() == 2);

    auto start = std::chrono::system_clock::now();
    for (auto i = 0u; i < LOOP_COUNT; ++i)
    {
        std::stringstream s;
        s << "loop " << i;
        v.push_back(s.str());
    }
    auto end = std::chrono::system_clock::now();
    CHECK((end - start) / 1s < 1);
    CHECK(v.size() == LOOP_COUNT + 2);
}

void run_test_two(const std::filesystem::path& p) {
    PersistentVector v(p);

    CHECK(v.size() == LOOP_COUNT + 2);
    CHECK(v.at(0) == "foo");
    CHECK(v.at(1) == all_chars());
    CHECK(v.at(873) == "loop 871");

    v.erase(873);
    CHECK(v.size() == LOOP_COUNT + 1);
    CHECK(v.at(0) == "foo");
    CHECK(v.at(1) == all_chars());
    CHECK(v.at(873) == "loop 872");
}

void run_test_three(const std::filesystem::path& p) {
    PersistentVector v(p);

    CHECK(v.size() == LOOP_COUNT + 1);
    CHECK(v.at(0) == "foo");
    CHECK(v.at(1) == all_chars());
    CHECK(v.at(873) == "loop 872");

    v.erase(873);
    CHECK(v.size() == LOOP_COUNT);
    CHECK(v.at(0) == "foo");
    CHECK(v.at(1) == all_chars());
    CHECK(v.at(873) == "loop 873");
}

int main(int argc, char**) {
    std::filesystem::path data_dir("data_dir");
    std::filesystem::create_directory(data_dir);

    run_test_one(data_dir);
    run_test_two(data_dir);
    run_test_three(data_dir);

    if (errors != 0) {
        std::cout << "tests were failing\n";
        return 1;
    }

    std::cout << "tests succeeded\n";
    return 0;
}
