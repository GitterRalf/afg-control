#pragma once
#include <QByteArray>
#include <QString>
#include <memory>

struct GpibTarget
{
    int board = 0;
    int address = 12;
};

class GpibSession
{
public:
    virtual ~GpibSession() = default;
    virtual void write(const QByteArray &command) = 0;
    virtual unsigned char status() = 0;
    virtual void local() = 0;
};

std::unique_ptr<GpibSession> openGpib(GpibTarget target);
