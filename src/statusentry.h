#pragma once
#include <memory>

class QAction;

namespace petdesk {
// Owns the platform's background entry; actions remain owned by the controller.
class StatusEntry final {
public:
    StatusEntry(QAction *open, QAction *importPet, QAction *visibility, QAction *pause, QAction *quit);
    ~StatusEntry();
    void activateApplication();
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
