#include <gtest/gtest.h>
#include <string_view>

#include "../Parser.hpp"
#include "../MimeTypes.hpp"
#include "../PathUtils.hpp"

// Mime Types Test
TEST(MimeTypesTest, ResolvesCommonWebExtensions) {
    EXPECT_EQ(MimeTypes::get_type("index.html"), "text/html");
    EXPECT_EQ(MimeTypes::get_type("styles.css"), "text/css");
    EXPECT_EQ(MimeTypes::get_type("cat.jpg"), "image/jpeg");
    EXPECT_EQ(MimeTypes::get_type("cat.jpeg"), "image/jpeg");
    EXPECT_EQ(MimeTypes::get_type("script.js"), "application/javascript");
}

TEST(MimeTypesTest, HandlesUnknownOrMissingExtensions) {
    // Unknown extensions should fallback to a safe binary stream or text
    std::string_view unknown = MimeTypes::get_type("firmware.bin");
    EXPECT_FALSE(unknown.empty());

    // File with no extension
    std::string_view no_ext = MimeTypes::get_type("LICENSE");
    EXPECT_FALSE(no_ext.empty());
}

// Http Parser test, return value

TEST(ParserStatelessTest, DetectsIncompleteHeaders) {
    Http::Request req;
    req.reset();

    // Partial payload without the final \r\n\r\n delimiter (EAGAIN simulation)
    std::string_view chunk = "GET /index.html HTTP/1.1\r\nHost: localhost";
    
    Http::ParseState state = Http::header_parser(chunk, req);
    
    // Must return INCOMPLETE so the epoll worker knows to wait for more data
    EXPECT_EQ(state, Http::ParseState::INCOMPLETE);
}

TEST(ParserStatelessTest, RejectsMalformedRequestLine) {
    Http::Request req;
    req.reset();

    // Corrupted HTTP request line
    std::string_view bad_req = "GARBAGE_REQUEST_WITHOUT_SPACES\r\n\r\n";
    
    Http::ParseState state = Http::header_parser(bad_req, req);
    EXPECT_EQ(state, Http::ParseState::ERROR);
}

// HTTP parser test

class HttpParserFixture : public ::testing::Test {
protected:
    Http::Request req;

    // Runs before every single TEST_F in this suite
    void SetUp() override {
        req.reset();
    }

    // Runs after every single TEST_F in this suite
    void TearDown() override {
        req.reset();
    }
};

TEST_F(HttpParserFixture, ParsesValidGetRequest) {
    std::string_view raw = 
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: curl/7.81.0\r\n"
        "Accept: */*\r\n\r\n";

    Http::ParseState state = Http::header_parser(raw, req);

    // Fatal check: if state is not COMPLETE, dereferencing parsed fields is pointless
    ASSERT_EQ(state, Http::ParseState::COMPLETE);

    // Non-fatal checks: evaluate all fields and report all failures in one run
    EXPECT_EQ(req.method, "GET");
    EXPECT_EQ(req.uri, "/index.html");
    EXPECT_EQ(req.version_major, 1);
    EXPECT_EQ(req.version_minor, 1);
    EXPECT_TRUE(req.keep_alive);
}

TEST_F(HttpParserFixture, ParsesConnectionCloseHeader) {
    std::string_view raw = 
        "GET /cat.jpg HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n\r\n";

    Http::ParseState state = Http::header_parser(raw, req);
    ASSERT_EQ(state, Http::ParseState::COMPLETE);
    EXPECT_FALSE(req.keep_alive);
}

TEST_F(HttpParserFixture, ExtractsContentLengthCorrectly) {
    std::string_view raw = 
        "POST /api/upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 42\r\n\r\n";

    Http::ParseState state = Http::header_parser(raw, req);
    ASSERT_EQ(state, Http::ParseState::COMPLETE);
    EXPECT_EQ(req.content_length, 42);
}

// Path Utils test

TEST(PathUtilsTest, HandlesInvalidOrNonExistentPath) {
    // Querying an invalid or non-cached file should safely return nullptr
    const std::string* path = File::get_abs_path("/definitely_does_not_exist_xyz123.html");
    EXPECT_EQ(path, nullptr);
}

TEST(PathUtilsTest, HandlesSecurityTraversalAttempts) {
    // Directory traversal attacks must not resolve outside the public root
    const std::string* attack_path = File::get_abs_path("/../../../../etc/passwd");
    EXPECT_EQ(attack_path, nullptr);
}