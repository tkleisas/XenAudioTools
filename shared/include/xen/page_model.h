// SPDX-License-Identifier: MIT OR GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "xen/upic.h"

namespace xen {

std::string serializePage(const Page& page);
bool deserializePage(const std::string& text, Page& page);

class PageModel
{
public:
    using Publisher = std::function<void(std::shared_ptr<const Page>)>;

    PageModel()
    {
        page_.arcs.push_back(Arc::point(0.0, 4.0, 0.5f, WaveformType::Gendyn));
        page_.updateDuration();
        page_.loopEnd = page_.duration;
        cachedText_ = serializePage(page_);
    }

    void setPublisher(Publisher publisher)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        publisher_ = std::move(publisher);
    }

    Page& pageRef() { return page_; }
    const Page& pageRef() const { return page_; }

    Page snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return page_;
    }

    void replace(Page page)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        page_ = std::move(page);
        cachedText_ = serializePage(page_);
        ++revision_;
    }

    void publish()
    {
        std::shared_ptr<const Page> snapshot;
        Publisher publisher;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++revision_;
            cachedText_ = serializePage(page_);
            snapshot = std::make_shared<const Page>(page_);
            publisher = publisher_;
        }
        if (publisher)
            publisher(std::move(snapshot));
    }

    uint64_t revision() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return revision_;
    }

    std::string serialize() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return cachedText_;
    }

    bool deserialize(const std::string& text)
    {
        Page loaded;
        if (!deserializePage(text, loaded))
            return false;
        std::lock_guard<std::mutex> lock(mutex_);
        page_ = std::move(loaded);
        cachedText_ = serializePage(page_);
        ++revision_;
        return true;
    }

private:
    mutable std::mutex mutex_;
    Page page_;
    std::string cachedText_;
    Publisher publisher_;
    uint64_t revision_ = 0;
};

} // namespace xen
