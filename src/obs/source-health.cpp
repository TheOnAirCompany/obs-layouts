/*
Layouts for OBS
Copyright (C) 2026 The OnAir Company

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "obs/source-health.hpp"

#include <obs.h>

#include <QElapsedTimer>
#include <QMetaObject>

namespace layouts {

namespace {

const char *const kDownSignals[] = {"media_ended", "media_stopped"};
const char *const kUpSignals[] = {"media_started", "media_play", "media_restart"};

int64_t nowMs()
{
	static QElapsedTimer clock = [] {
		QElapsedTimer t;
		t.start();
		return t;
	}();
	return clock.elapsed();
}

} // namespace

struct SourceHealth::Watched {
	std::string name;
	obs_weak_source_t *weak = nullptr;
	bool lost = false;
	bool mediaDown = false;
	int64_t badSince = -1;
	int64_t goodSince = -1;

	/* Media signals come from OBS threads: only the name is copied, the rest happens on
	 * the UI thread. */
	static void onDown(void *data, calldata_t *)
	{
		const std::string n = static_cast<Watched *>(data)->name;
		QMetaObject::invokeMethod(
			&SourceHealth::instance(), [n] { SourceHealth::instance().setMediaDown(n, true); },
			Qt::QueuedConnection);
	}
	static void onUp(void *data, calldata_t *)
	{
		const std::string n = static_cast<Watched *>(data)->name;
		QMetaObject::invokeMethod(
			&SourceHealth::instance(), [n] { SourceHealth::instance().setMediaDown(n, false); },
			Qt::QueuedConnection);
	}

	void connect(obs_source_t *source)
	{
		weak = obs_source_get_weak_source(source);
		signal_handler_t *sh = obs_source_get_signal_handler(source);
		for (const char *s : kDownSignals)
			signal_handler_connect(sh, s, onDown, this);
		for (const char *s : kUpSignals)
			signal_handler_connect(sh, s, onUp, this);
	}

	~Watched()
	{
		/* Disconnecting waits for a signal being delivered, so no callback can use this
		 * object afterwards. A destroyed source took its handler with it. */
		obs_source_t *source = weak ? obs_weak_source_get_source(weak) : nullptr;
		if (source) {
			signal_handler_t *sh = obs_source_get_signal_handler(source);
			for (const char *s : kDownSignals)
				signal_handler_disconnect(sh, s, onDown, this);
			for (const char *s : kUpSignals)
				signal_handler_disconnect(sh, s, onUp, this);
			obs_source_release(source);
		}
		obs_weak_source_release(weak);
	}
};

SourceHealth &SourceHealth::instance()
{
	static SourceHealth *health = new SourceHealth; /* lives until the process ends */
	return *health;
}

SourceHealth::SourceHealth()
{
	timer_.setInterval(health::kSampleMs);
	connect(&timer_, &QTimer::timeout, this, &SourceHealth::sample);
}

SourceHealth::~SourceHealth() = default;

void SourceHealth::watch(const std::vector<std::string> &names)
{
	std::map<std::string, std::unique_ptr<Watched>> next;
	for (const std::string &n : names) {
		if (n.empty() || next.count(n))
			continue;
		auto it = watched_.find(n);
		if (it != watched_.end()) {
			next[n] = std::move(it->second);
			continue;
		}
		auto w = std::make_unique<Watched>();
		w->name = n;
		obs_source_t *source = obs_get_source_by_name(n.c_str());
		if (source) {
			w->connect(source);
			obs_source_release(source);
		}
		next[n] = std::move(w);
	}
	watched_ = std::move(next); /* the ones left out disconnect here */
	if (watched_.empty())
		timer_.stop();
	else if (!timer_.isActive())
		timer_.start();
}

void SourceHealth::clear()
{
	watch({});
}

bool SourceHealth::isLost(const std::string &name) const
{
	auto it = watched_.find(name);
	return it != watched_.end() && it->second->lost;
}

void SourceHealth::setMediaDown(const std::string &name, bool down)
{
	auto it = watched_.find(name);
	if (it == watched_.end())
		return;
	it->second->mediaDown = down;
	sample();
}

void SourceHealth::sample()
{
	const int64_t now = nowMs();
	std::vector<std::pair<QString, bool>> changes;
	for (auto &[name, w] : watched_) {
		bool bad = true;
		obs_source_t *source = w->weak ? obs_weak_source_get_source(w->weak) : nullptr;
		if (!source) {
			/* Gone, or created after the watch began: connect to the current one. */
			obs_weak_source_release(w->weak);
			w->weak = nullptr;
			source = obs_get_source_by_name(name.c_str());
			if (source)
				w->connect(source);
		}
		if (source) {
			const enum obs_media_state media = obs_source_media_get_state(source);
			bad = obs_source_get_width(source) == 0 || obs_source_get_height(source) == 0 || w->mediaDown ||
			      media == OBS_MEDIA_STATE_ENDED || media == OBS_MEDIA_STATE_ERROR;
			obs_source_release(source);
		}
		if (bad) {
			w->goodSince = -1;
			if (w->badSince < 0)
				w->badSince = now;
			if (!w->lost && now - w->badSince >= health::kLostAfterMs) {
				w->lost = true;
				changes.push_back({QString::fromStdString(name), true});
			}
		} else {
			w->badSince = -1;
			if (w->goodSince < 0)
				w->goodSince = now;
			if (w->lost && now - w->goodSince >= health::kBackAfterMs) {
				w->lost = false;
				changes.push_back({QString::fromStdString(name), false});
			}
		}
	}
	/* Emitted after the loop: a receiver may change the watched list. */
	for (const auto &[name, lost] : changes) {
		if (lost)
			emit sourceLost(name);
		else
			emit sourceBack(name);
	}
}

} // namespace layouts
