#include "NetListener.h"

#include <net/PrimitiveType.h>
#include <common/logger.h>

#ifdef __APPLE__
#include <errno.h>
#endif


NetListener::NetListener(Scene *scene, const std::string &listen_host, uint16_t listen_port)
    : scene_(scene) {
    socket_ = std::make_unique<CPassiveSocket>(CPassiveSocket::SocketTypeTcp);
    if (!socket_->Initialize()) {
        LOG_ERROR("NetListener:: Cannot initialize socket: %d", errno);
    } else {
        socket_->DisableNagleAlgoritm();
        if (!socket_->Listen(reinterpret_cast<const uint8_t *>(listen_host.data()), listen_port)) {
            LOG_ERROR("NetListener:: Cannot listen on socket: %d", errno);
        }
    }

    status_ = ConStatus::CLOSED;
}

NetListener::ConStatus NetListener::connection_status() const {
    return status_;
}

void NetListener::run() {
    status_ = ConStatus::WAIT;
    LOG_INFO("NetClient:: Start listening");

    client_ = socket_->Accept();
    if (!client_) {
        status_ = ConStatus::CLOSED;
        char buf[256];
        snprintf(buf, sizeof(buf), "Accept on socket returned NULL. errno=%d; %s", errno, strerror(errno));
        throw std::runtime_error(buf);
    } else {
        LOG_INFO("NetListener:: Got connection from %s:%d", client_->GetClientAddr(), client_->GetClientPort());
    }

    status_ = ConStatus::ESTABLISHED;

    while (!stop_) {
        try {
            /// формат сообщений одинаковый: один байт, буква - тип, потом данные под него.
            const auto message_type_of_str = read_bytes(1)[0];
            assert(PrimitiveType::begin == primitive_type_from_char(message_type_of_str));
            const auto packageSize = read_uint32();

            if (stop_) {
                break;
            }

            const auto packageBody = read_bytes(packageSize);

            LOG_V2("NetClient:: Received package with size: '%d'", packageSize);
            process_message(packageBody);
        } catch (const std::exception& e) {
            LOG_WARN("NetListener::Exception: %s", e.what());
            break;
        }
    }
}

void NetListener::stop() {
    if (status_ != ConStatus::CLOSED) {
        LOG_INFO("Stopping network listening")
    }
    stop_ = true;
}



/*
* Data deserialisation
*/
namespace pod {

    void to_color(uint32_t color, Colored &p) {
        p.color.r = ((color & 0xFF0000) >> 16) / 256.0f;
        p.color.g = ((color & 0x00FF00) >> 8) / 256.0f;
        p.color.b = ((color & 0x0000FF)) / 256.0f;
    }

    void to_line(float x1, float y1, float x2, float y2, Line &p) {
        p.x1 = x1;
        p.y1 = y1;
        p.x2 = x2;
        p.y2 = y2;
    }

    void to_circle(float x, float y, float r, Circle &p) {
        p.radius = r;
        p.center.x = x;
        p.center.y = y;
    }

    void to_rect(float x1, float y1, float x2, float y2, Rectangle &p) {
        p.w = x2 - x1;
        p.h = y1 - y2;
        p.center.x = x1 + p.w * 0.5f;
        p.center.y = y2 + p.h * 0.5f;
    }

    void to_unit(uint32_t hp, uint32_t max_hp, int16_t utype, int16_t is_enemy, float course, Unit &p) {
        static const glm::vec3 our_color { 0.0f, 0.0f, 1.0f };
        static const glm::vec3 enemy_color { 1.0f, 0.0f, 0.0f };
        static const glm::vec3 neutral_color { 0.5f, 0.5f, 0.0f };

        if (is_enemy == 1) {
            p.color = enemy_color;
        } else if (is_enemy == -1) {
            p.color = our_color;
        } else {
            p.color = neutral_color;
        }

        p.hp = hp;
        p.max_hp = max_hp;

        if (0 <= utype && utype < static_cast<int16_t>(Frame::UnitType::count)) {
            p.utype = static_cast<Frame::UnitType>(utype);
        } else {
            p.utype = Frame::UnitType::undefined;
        }

        p.course = course;
    }

    void to_area(uint32_t x, uint32_t y, int16_t area_type, AreaDesc &p) {
        p.x = x;
        p.y = y;
        p.type = static_cast<Frame::AreaType>(area_type);
    }

};

void NetListener::process_message(const std::vector<signed char>& package) {
    if (package.empty()) {
        return;
    }

    if (!frame_) {
        frame_ = std::make_unique<Frame>();
    }

    const signed char* iter= &package[0];
    const signed char* end = &package[package.size()-1];

    while (iter <= end) {
        /// тип, а потом данные
        auto type_of_str = *(iter++);
        auto type = primitive_type_from_char(type_of_str);

        switch (type) {
            case PrimitiveType::begin:
                LOG_DEBUG("NetClient::Begin");
                break;

            case PrimitiveType::end:
                LOG_DEBUG("NetClient::End");
                if (!stop_) {
                    scene_->add_frame(std::move(frame_));
                }
                frame_ = nullptr;
                break;

            case PrimitiveType::circle:
            {
                LOG_DEBUG("NetClient::Circle detected");
                auto circle = pod::Circle();
                auto x = read_float(&iter); auto y = read_float(&iter); auto r = read_float(&iter);
                pod::to_circle(x, y, r, circle);
                pod::to_color(read_uint32(&iter), circle);
                frame_->circles.emplace_back(circle);
                break;
            }

            case PrimitiveType::rectangle:
            {
                LOG_DEBUG("NetClient::Rectangle detected");
                auto rectangle = pod::Rectangle();
                auto x1 = read_float(&iter); auto y1 = read_float(&iter); auto x2 = read_float(&iter); auto y2 = read_float(&iter);
                pod::to_rect(x1, y1, x2, y2, rectangle);
                pod::to_color(read_uint32(&iter), rectangle);
                frame_->rectangles.emplace_back(rectangle);
                break;
            }

            case PrimitiveType::line:
            {
                LOG_DEBUG("NetClient::Line detected");
                auto line = pod::Line();
                auto x1 = read_float(&iter); auto y1 = read_float(&iter); auto x2 = read_float(&iter); auto y2 = read_float(&iter);
                pod::to_line(x1, y1, x2, y2, line);
                pod::to_color(read_uint32(&iter), line);
                line.surprise = line.color;
                frame_->lines.emplace_back(line);
                break;
            }

            case PrimitiveType::message:
            {
                LOG_DEBUG("NetClient::Message");
                auto length = read_uint32(&iter);
                auto bytes = read_bytes(length);
                frame_->user_message = std::string(bytes.begin(), bytes.end());
                break;
            }

            case PrimitiveType::unit:
            {
                LOG_DEBUG("NetClient::Unit");
                auto unit = pod::Unit();
                auto x = read_float(&iter); auto y = read_float(&iter); auto r = read_float(&iter);
                pod::to_circle(x, y, r, unit);
                auto hp = read_uint32(&iter); auto max_hp = read_uint32(&iter); auto utype = read_int16(&iter);
                auto enemy = read_int16(&iter); auto course = read_float(&iter);
                pod::to_unit(hp, max_hp, utype, enemy, course, unit);

                frame_->units.emplace_back(unit);
                break;
            }

            case PrimitiveType::area:
            {
                LOG_DEBUG("NetClient::Area");
                auto area = pod::AreaDesc();
                auto x = read_float(&iter); auto y = read_float(&iter); auto type = read_int16(&iter);
                pod::to_area(x, y, type, area);

                scene_->add_area_description(area);
                break;
            }

            case PrimitiveType::types_count:
                assert(false);
                break;
        }
    }    
}


std::vector<signed char> NetListener::read_bytes(unsigned int byteCount) {
    std::vector<signed char> bytes(byteCount);
    unsigned int offset = 0;
    int receivedByteCount;

    while (offset < byteCount && (receivedByteCount = client_->Receive(byteCount - offset)) > 0) {
        memcpy(&bytes[offset], client_->GetData(), receivedByteCount);
        offset += receivedByteCount;
    }

    if (offset != byteCount) {
        client_->Close();
        status_ = ConStatus::CLOSED;
        throw std::exception("incorrect format");
    }

    return bytes;
}

bool isLittleEndianMachine() {
    union {
        uint16 value;
        unsigned char bytes[2];
    } test = {0x0201};

    return test.bytes[0] == 1;
}

uint32_t NetListener::read_uint32() {
    std::vector<signed char> bytes = this->read_bytes(4);

    if (!isLittleEndianMachine()) {
        reverse(bytes.begin(), bytes.end());
    }

    uint32_t value;
    memcpy(&value, &bytes[0], 4);
    return value;
}

uint32_t NetListener::read_uint32(const signed char** iterator) {
    signed char buf[4] = { 0x0 };
    

    if (!isLittleEndianMachine()) {
        buf[0] = *(*iterator + 3);
        buf[1] = *(*iterator + 2);
        buf[2] = *(*iterator + 1);
        buf[3] = *(*iterator + 0);
    } else {
        buf[0] = *(*iterator + 0);
        buf[1] = *(*iterator + 1);
        buf[2] = *(*iterator + 2);
        buf[3] = *(*iterator + 3);
    }

    *iterator += 4;

    uint32_t value;
    memcpy(&value, &buf[0], 4);
    return value;
}

uint16_t NetListener::read_int16(const signed char** iterator) {
    signed char buf[2] = { 0x0 };


    if (!isLittleEndianMachine()) {
        buf[0] = *(*iterator + 3);
        buf[1] = *(*iterator + 2);
    } else {
        buf[0] = *(*iterator + 0);
        buf[1] = *(*iterator + 1);
    }

    *iterator += 4;

    int16_t value;
    memcpy(&value, &buf[0], 2);
    return value;
}

float NetListener::read_float(const signed char** iterator) {
    signed char buf[4] = { 0x0 };


    if (!isLittleEndianMachine()) {
        buf[0] = *(*iterator + 3);
        buf[1] = *(*iterator + 2);
        buf[2] = *(*iterator + 1);
        buf[3] = *(*iterator + 0);
    } else {
        buf[0] = *(*iterator + 0);
        buf[1] = *(*iterator + 1);
        buf[2] = *(*iterator + 2);
        buf[3] = *(*iterator + 3);
    }

    *iterator += 4;

    float value;
    memcpy(&value, &buf[0], 4);
    return value;
}
