#include "application_tests.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>

static void fixture(const std::string& path, const std::string& value)
{
    std::ofstream file(path.c_str(), std::ios::binary);
    file.write(value.data(), value.size());
    assert(file.good());
}

static std::string readFile(const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    std::ostringstream data;
    data << file.rdbuf();
    assert(file.good() || file.eof());
    return data.str();
}

static void drive(CgiProcess& process)
{
    while (!process.finished())
    {
        pollfd fds[2];
        fds[0].fd = process.inputFd(); fds[0].events = POLLOUT; fds[0].revents = 0;
        fds[1].fd = process.outputFd(); fds[1].events = POLLIN; fds[1].revents = 0;
        const int ready = poll(fds, 2, 50);
        assert(ready >= 0);
        if (fds[0].revents & POLLOUT) process.onWritable();
        else if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) process.onPipeError();
        if (fds[1].revents & POLLIN) process.onReadable();
        else if (fds[1].revents & POLLHUP) process.onOutputHangup();
        else if (fds[1].revents & (POLLERR | POLLNVAL)) process.onPipeError();
        process.tick(std::time(NULL));
    }
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    signal(SIGPIPE, SIG_IGN);
    const std::string root = argv[1];
    const std::string interpreter = argv[2];
    const std::string unusual = "a & \"#%.txt";
    fixture(root + "/" + unusual, "special");
    fixture(root + "/plain.txt", "plain");
    fixture(root + "/..notes.txt", "notes");

    ServerConfig config;
    config.setServerName("localhost"); config.setMaxBodySize(100000);
    Location base; base.setPath("/"); base.setRoot(root);
    base.setUploadDirectory(root); base.setAutoindex(true);
    base.allowedMethod("GET"); base.allowedMethod("POST"); base.allowedMethod("DELETE");
    config.addLocation(base);
    Location files = base; files.setPath("/files/"); config.addLocation(files);
    Location uploads = base; uploads.setPath("/upload"); config.addLocation(uploads);
    Location cgi = base; cgi.setPath("/cgi-bin"); cgi.setRoot(root + "/cgi-bin");
    cgi.setCgiExtension(".py"); cgi.setCgiPath(interpreter); config.addLocation(cgi);

    RequestHandler handler;
    HTTPRequest request; request.method = "GET"; request.uri = "/files/plain.txt";
    assert(handler.handleRequest(request, config).body == "plain");
    request.uri = "/filesX/plain.txt";
    assert(handler.handleRequest(request, config).statusCode == 404);
    request.uri = "/a%20%26%20%22%23%25.txt";
    assert(handler.handleRequest(request, config).body == "special");
    request.uri = "/..notes.txt";
    assert(handler.handleRequest(request, config).body == "notes");
    request.uri = "/%2e%2e/plain.txt";
    assert(handler.handleRequest(request, config).statusCode == 403);
    request.uri = "/plain%00.txt";
    assert(handler.handleRequest(request, config).statusCode == 400);
    request.uri = "/bad%2";
    assert(handler.handleRequest(request, config).statusCode == 400);
    request.uri = "/";
    HTTPResponse listing = handler.handleRequest(request, config);
    assert(listing.body.find("a &amp; &quot;#%.txt") != std::string::npos);
    assert(listing.body.find("./a%20%26%20%22%23%25.txt") != std::string::npos);
    request.method = "POST"; request.uri = "/fresh.txt"; request.body = "raw";
    assert(handler.handleRequest(request, config).statusCode == 201);
    assert(readFile(root + "/fresh.txt") == "raw");
    assert(handler.handleRequest(request, config).statusCode == 403); // No silent overwrite.
    assert(mkdir((root + "/nested").c_str(), 0755) == 0);
    request.uri = "/upload/nested/inside.txt"; request.body = "nested upload";
    assert(handler.handleRequest(request, config).statusCode == 201);
    assert(readFile(root + "/nested/inside.txt") == "nested upload");
    request.uri = "/upload/missing/inside.txt";
    assert(handler.handleRequest(request, config).statusCode == 404);
    request.uri = "/"; request.body = "--B\r\nContent-Disposition: form-data; name=\"file\"; filename=\"form.txt\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n";
    std::string payload("a\0b", 3);
    payload += "\r\n--Bnot-a-boundary";
    request.body += payload + "\r\n--B--\r\n";
    request.headers["content-type"] = "multipart/form-data; boundary=\"B\"";
    assert(handler.handleRequest(request, config).statusCode == 201);
    assert(readFile(root + "/form.txt") == payload);
    MultipartUpload upload;
    const std::string unsafe = "--B\r\nContent-Disposition: form-data; name=\"file\"; filename=\"../escape\"\r\n\r\nx\r\n--B--\r\n";
    assert(!upload.parse(request.headers["content-type"], unsafe));
    assert(!upload.parse("multipart/form-data; boundary=", request.body));
    assert(!upload.parse(request.headers["content-type"], request.body.substr(0, request.body.size() - 6)));
    request.headers.clear(); request.uri = "/too-large.txt"; request.body.assign(100001, 'x');
    assert(handler.handleRequest(request, config).statusCode == 413);

    request.method = "GET"; request.uri = "/cgi-bin/hello.py"; request.body.clear();
    assert(handler.handleRequest(request, config).statusCode == 501); // No source-code disclosure.
    CgiContext context; context.query = "name=Arina%20%26%20team";
    context.serverPort = 8080; context.remoteAddress = "127.0.0.1";
    CgiRequest plan; HTTPResponse error;
    assert(handler.prepareCgi(request, config, context, plan, error) == RequestHandler::CGI_READY);
    CgiProcess process;
    assert(process.start(plan.interpreter, plan.scriptPath, plan.environment, plan.body, std::vector<int>()));
    drive(process);
    assert(process.errorStatus() == 0);
    HTTPResponse cgiResponse = handler.parseCgiOutput(process.output());
    assert(cgiResponse.statusCode == 200);
    assert(cgiResponse.body.find("Hello, Arina &amp; team!") != std::string::npos);
    assert(process.output().find("Hello, Arina &amp; team!") != std::string::npos);
    assert(process.output().find("Relative file: CGI relative path works") != std::string::npos);

    request.method = "POST"; request.body = "hello CGI";
    assert(handler.prepareCgi(request, config, context, plan, error) == RequestHandler::CGI_READY);
    CgiProcess posted;
    assert(posted.start(plan.interpreter, plan.scriptPath, plan.environment, plan.body, std::vector<int>()));
    drive(posted);
    assert(posted.errorStatus() == 0 && posted.output().find("hello CGI") != std::string::npos);

    HTTPResponse eof = handler.parseCgiOutput("Content-Type: text/plain\n\nEOF body");
    assert(eof.statusCode == 200 && eof.body == "EOF body");
    assert(eof.headers["Content-Length"] == "8");
    HTTPResponse status = handler.parseCgiOutput("Status: 201 Created\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nok");
    assert(status.statusCode == 201 && status.body == "ok");
    assert(status.headers.count("Status") == 0);
    assert(handler.parseCgiOutput("Location: /new-page/\n\n").statusCode == 302);
    assert(handler.parseCgiOutput("Content-Type: text/plain\nContent-Length: 9\n\nx").statusCode == 500);
    assert(handler.parseCgiOutput("Content-Type: text/plain\nContent-Length: 99999999999999999999999999\n\nx").statusCode == 500);
    assert(handler.parseCgiOutput("Status: 999 Invalid\n\n").statusCode == 500);
    assert(handler.parseCgiOutput("not a CGI response").statusCode == 500);
    assert(handler.parseCgiOutput("Content-Type: text/plain\nX-Bad: a\rb\n\nx").statusCode == 500);
    assert(handler.parseCgiOutput("Content-Type: text/plain\ncontent-type: text/html\n\nx").statusCode == 500);
    assert(handler.parseCgiOutput("Content-Type: text/plain\nTransfer-Encoding: chunked\n\nx").statusCode == 500);
    HTTPResponse hop = handler.parseCgiOutput("Content-Type: text/plain\nConnection: X-Private\nX-Private: secret\n\nx");
    assert(hop.statusCode == 200 && hop.headers.count("X-Private") == 0);
    std::string binary("a\0b", 3);
    HTTPResponse binaryResponse = handler.parseCgiOutput("Content-Type: application/octet-stream\r\n\r\n" + binary);
    assert(binaryResponse.body == binary && binaryResponse.headers["Content-Length"] == "3");
    assert(handler.parseCgiOutput("Status: 204 No Content\n\n").body.empty());
    // Multiple pipe reads and final HUP must retain every byte of EOF-delimited output.
    fixture(root + "/cgi-bin/large.py", "import sys\nsys.stdout.write('Content-Type: text/plain\\n\\n' + 'x' * 32768)\n");
    CgiProcess large;
    assert(large.start(interpreter, root + "/cgi-bin/large.py", plan.environment, "", std::vector<int>()));
    drive(large);
    assert(large.errorStatus() == 0);
    HTTPResponse largeResponse = handler.parseCgiOutput(large.output());
    assert(largeResponse.statusCode == 200 && largeResponse.body == std::string(32768, 'x'));
    fixture(root + "/cgi-bin/slow.py", "import time\ntime.sleep(60)\n");
    CgiProcess slow;
    assert(slow.start(interpreter, root + "/cgi-bin/slow.py", plan.environment, "", std::vector<int>(), 1));
    drive(slow);
    assert(slow.errorStatus() == 504);
    fixture(root + "/cgi-bin/fail.py", "raise RuntimeError('expected test failure')\n");
    CgiProcess failed;
    assert(failed.start(interpreter, root + "/cgi-bin/fail.py", plan.environment, "", std::vector<int>()));
    drive(failed);
    assert(failed.errorStatus() == 500);
    std::cout << "Application and CGI component checks passed" << std::endl;
}
