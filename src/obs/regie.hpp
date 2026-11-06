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

/* Switcher ("Régie"): a vision mixer for one "Plateau" scene that holds every camera,
 * screen and chat of the show. The Switcher recomposes it: which broadcast layout, and
 * which source goes in which slot.
 *
 * Preview + TAKE, with OBS Studio Mode:
 *  - The next composition is prepared on the scene that is NOT on program, then shown
 *    in the OBS preview. TAKE runs the Studio Mode transition.
 *  - With "Duplicate scene" on (the OBS default), the program shows a private copy of
 *    the Plateau, so the Plateau itself can be prepared even while it is on air.
 *  - Without duplication, a twin scene "<Plateau> B" (same sources, shared) is used in
 *    ping-pong: the composition goes on whichever of the two is not on program.
 *  - With Studio Mode off there is no preview: changes go on air at once (the dock
 *    offers to turn Studio Mode on).
 * Items not used by the layout are hidden, never removed. Graphics, backgrounds and
 * untyped sources are left alone. */

#include "core/broadcast.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include "morph.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

#include <string>
#include <vector>

namespace layouts {

struct RegieComposition {
	std::string key;                /* broadcast template */
	std::vector<std::string> tiles; /* source name per slot, empty = nothing */
	bool operator==(const RegieComposition &o) const { return key == o.key && tiles == o.tiles; }
};

struct RegieSource {
	std::string name;
	core::SlotKind kind;
};

class Regie : public QObject {
	Q_OBJECT

public:
	static Regie &instance();

	/* ---- the Plateau scene (saved with the scene collection) ---- */
	QString plateau() const { return plateau_; }
	void setPlateau(const QString &sceneName);
	OBSSource plateauSource() const;

	/* Cameras, screens and chats of the Plateau (top-level items), in scene order from the
	 * top of the list. */
	const std::vector<RegieSource> &sources() const { return sources_; }
	/* Templates that fit the Plateau's counts. */
	const std::vector<core::BroadcastTemplate> &layouts() const { return layouts_; }
	/* Slots of a template (kinds and cells on the Plateau canvas). */
	std::vector<core::Slot> slotsOf(const std::string &key) const;
	double canvasAspect() const;

	const RegieComposition &preview() const { return preview_; }
	const RegieComposition &program() const { return program_; }
	bool studioMode() const;
	/* The source is in a slot of the program / preview composition. */
	bool onProgram(const std::string &source) const;
	bool onPreview(const std::string &source) const;

	/* ---- actions (prepare the preview; with Studio Mode off they go on air) ---- */
	bool setLayout(const std::string &key);
	bool nextLayout(int step);
	bool favorite(int index); /* 0-based position in layouts() */
	/* Puts a source in a slot; when it already is in another slot, the two swap. */
	bool assign(int slot, const std::string &source);
	bool swap(int a, int b);
	/* Preview to program (Studio Mode transition). */
	bool take();
	/* AUTO (0.6): animates the program scene to the preview composition. */
	bool autoTake();
	/* Dual output: refresh the 9:16 twin and put it on the vertical program. */
	void cutVertical(obs_source_t *landscape);

	/* Re-reads the Plateau (sources, counts, current layout guess). */
	void refresh();

	void save(obs_data_t *data);
	void load(obs_data_t *data);
	void handleFrontendEvent(enum obs_frontend_event event);

	void registerHotkeys();
	void saveHotkeys();
	void unregisterHotkeys();

	/* obs-websocket vendor "layouts" (call from obs_module_post_load). */
	void registerVendor();
	void unregisterVendor();
	/* JSON-like state for GetState and the StateChanged event. */
	void stateTo(obs_data_t *out) const;

signals:
	void changed();

private:
	Regie() = default;
	void scan();
	void fill(RegieComposition &c) const;
	bool prepare();
	bool apply(obs_source_t *scene, const RegieComposition &c);
	std::vector<morph::Target> targetsFor(obs_source_t *scene, const RegieComposition &c,
					      std::vector<std::pair<obs_sceneitem_t *, core::Slot>> &placed) const;
	obs_source_t *targetScene();
	OBSSource twin(bool create);
	void emitChanged();

	mutable QString plateau_; /* follows a rename (plateauSource) */
	std::string plateauUuid_; /* survives a rename of the Plateau scene */
	void armFailover();
	std::vector<RegieSource> sources_;
	std::vector<core::BroadcastTemplate> layouts_;
	RegieComposition preview_;
	RegieComposition program_;
	OBSWeakSource preparedOn_;
	void *vendor_ = nullptr;
};

} // namespace layouts
