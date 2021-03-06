#include "packets.h"
#include <bitset>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cxxopts.hpp>
#include <ios>
#include <sockpp/tcp_connector.h>
#include <sockpp/version.h>

using namespace metin2_clientless_bot;

static bool parse_command_line(int argc, char **&argv)
{
    try
    {
        cxxopts::Options options("Metin2-Clientless-Bot");
        options.positional_help("[optional args]").show_positional_help();
        options.allow_unrecognised_options().add_options()("i, ip", "-i <127.0.0.1>", cxxopts::value<std::string>())(
            "p, port", "-p <12345>", cxxopts::value<std::uint16_t>())("h, help", "Print help");

        const auto result = options.parse(argc, argv);
        const auto &arguments = result.arguments();

        if (arguments.empty() || result.count("help"))
        {
            std::cout << options.help() << std::endl;
            return true;
        }

        const auto ip = result.count("ip");
        const auto port = result.count("port");

        if (ip && port)
        {
            const auto &ip_arg = result["ip"].as<std::string>();
            const auto port_arg = result["port"].as<std::uint16_t>();

            sockpp::tcp_connector conn({ip_arg, port_arg});
            if (!conn)
            {
                std::cerr << "Error connecting to server at " << sockpp::inet_address(ip_arg, port_arg) << "\n"
                          << conn.last_error_str() << std::endl;
                return false;
            }

            std::cout << "Created a connection from " << conn.address() << std::endl;

            if (!conn.read_timeout(std::chrono::seconds(5)))
            {
                std::cerr << "Error setting timeout on TCP stream: " << conn.last_error_str() << std::endl;
                return false;
            }

            const packet::header_t state = packet::client::headers::header_channel_status;
            if (conn.write_n(&state, sizeof(state)) == -1)
            {
                std::cerr << "Error writing to the TCP stream: " << conn.last_error_str() << std::endl;
                return false;
            }
            std::cout << "Send " << sizeof(state) << " byte(s) to " << conn.peer_address() << " !" << std::endl;

            uint64_t count = 0;
            std::vector<packet::header_t> unknown_headers;
            while (true)
            {
                packet::header_t header;
                if (conn.read_n(&header, sizeof(header)) != -1)
                {
                    // if (header == 0)
                    //    continue;

                    bool handled = false;
                    switch (header)
                    {
                    case packet::server::headers::header_game_phase: {
                        uint8_t phase_type;
                        handled = conn.read_n(&phase_type, sizeof(phase_type)) != -1;
                        std::cout << "recv phase " << static_cast<signed>(phase_type) << std::endl;
                    }
                    break;

                    case packet::server::headers::header_channel_status: {
                        int32_t size;
                        if (conn.read_n(&size, sizeof(size)) != -1)
                        {
                            std::cout << size << " ports" << std::endl;
                            while (size--)
                            {
                                uint16_t port;
                                if (conn.read_n(&port, sizeof(port)) == -1)
                                {
                                    handled = false;
                                    break;
                                }

                                uint8_t status;
                                if (conn.read_n(&status, sizeof(status)) == -1)
                                {
                                    handled = false;
                                    break;
                                }
                                std::cout << "port " << port << " " << (status ? "ON" : "OFF") << std::endl;
                                handled = true;
                            }

                            if (!handled)
                                break;
                        }
                        else
                        {
                            handled = false;
                            break;
                        }
                        uint8_t succes;
                        handled = conn.read_n(&succes, sizeof(succes)) != -1;
                    }
                    break;

                    case packet::server::headers::header_handshake: {
                        uint32_t handshake;
                        handled = conn.read_n(&handshake, sizeof(handshake)) != -1;

                        if (!handled)
                            break;

                        std::cout << "handshake " << handshake << std::endl;

                        uint32_t time;
                        handled = conn.read_n(&time, sizeof(time)) != -1;

                        if (!handled)
                            break;

                        std::cout << "time " << time << std::endl;

                        int32_t delta;
                        handled = conn.read_n(&delta, sizeof(delta)) != -1;

                        if (!handled)
                            break;

                        std::cout << "delta " << delta << std::endl;
                    }
                    break;
                    // case packet::server::headers::header_ping: {
                    //    handled = true;
                    //}
                    case packet::server::headers::header_same_login: {
                        handled = true;
                    }
                    break;
                    default:
                        handled = false;
                        break;
                    }

                    if (!handled)
                    {
                        std::cout << "can't handle header " << static_cast<signed>(header) << std::endl;
                        unknown_headers.emplace_back(header);
                        // return false;
                    }
                    else
                    {
                        std::cout << "handled header " << static_cast<signed>(header) << std::endl;
                        // if (header == packet::server::headers::header_channel_status)
                        //{
                        //    if (conn.write_n(&state, sizeof(state)) == -1)
                        //    {
                        //        std::cerr << "Error writing to the TCP stream: " << conn.last_error_str() <<
                        //        std::endl; return false;
                        //    }
                        //    count++;
                        //}
                        if (header == 83)
                            break;
                    }
                }
            }
            std::cout << unknown_headers.data() << " :" << unknown_headers.size() << std::endl;
        }
        else
        {
            std::cout << options.help() << std::endl;
            return true;
        }
    }
    catch (const cxxopts::OptionException &e)
    {
        std::cerr << e.what() << std::endl;
        return false;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return false;
    }
    catch (...)
    {
        std::cerr << "unhandled exception" << std::endl;
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    sockpp::socket_initializer sock_init;
    return parse_command_line(argc, argv);
}
