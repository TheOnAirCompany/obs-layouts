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

#pragma once

/* Health of the camera and screen sources of a live layout: tells when one stops giving
 * a picture (NDI cut, capture device unplugged, display gone, media ended) and when it
 * comes back.
 *
 * How a drop is seen, with the public libobs API only:
 *  - Size 0 x 0. An async source (video capture, NDI, media) reports the size of its last
 *    frame only while it is active: when the producer signals the loss by sending an empty
 *    frame (obs_source_output_video(NULL), what capture devices and NDI do on a cut),
 *    obs_source_get_width/height fall to 0. Sync sources (screen / window capture) also
 *    report 0 when what they capture disappears. Sampled on the UI thread.
 *  - Media state ended / error, and the "media_ended" / "media_stopped" signals (these
 *    arrive on OBS threads and are queued to the UI thread).
 *  - The source no longer exists.
 * "deactivate" / "hide" are not used: they only mean the source left the program.
 * A frozen last frame (some NDI senders keep the last picture) is not seen: that needs a
 * probe filter on the source, and filters are saved in the scene collection.
 *
 * Debounce: lost after kLostAfterMs without a picture, back after kBackAfterMs of a
 * stable picture. */

#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace layouts {

namespace health {
constexpr int kSampleMs = 250;
constexpr int kLostAfterMs = 1500; /* provisional */
constexpr int kBackAfterMs = 2000; /* provisional */
} // namespace health

class SourceHealth : public QObject {
	Q_OBJECT

public:
	static SourceHealth &instance();

	/* Sources to watch, by name (replaces the previous list). Sources already lost and
	 * still watched stay lost. */
	void watch(const std::vector<std::string> &names);
	void clear();
	bool isLost(const std::string &name) const;

signals:
	void sourceLost(const QString &name);
	void sourceBack(const QString &name);

private:
	SourceHealth();
	~SourceHealth() override;
	struct Watched;
	void sample();
	void setMediaDown(const std::string &name, bool down);

	QTimer timer_;
	std::map<std::string, std::unique_ptr<Watched>> watched_;
};

} // namespace layouts
