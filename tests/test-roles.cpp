#include "test.hpp"

#include "roles.hpp"

using namespace layouts::core;

TEST("roles: the four show sources are recognized from their names")
{
	CHECK(roleFromName("CAM _ GUEST 1") == Role::Camera);
	CHECK(roleFromName("CAM _ VALENTIN") == Role::Camera);
	CHECK(roleFromName("SCREEN _ RIGHT") == Role::Screen);
	CHECK(roleFromName("BG _ DYNAMIQUE") == Role::Background);
	CHECK(roleFromName("HOST _ VALENTIN") == Role::Host);
	CHECK(roleFromName("Présentateur") == Role::Host);
	CHECK(isCameraRole(Role::Host) && isCameraRole(Role::Camera) && !isCameraRole(Role::Screen));
}

TEST("roles: case, accents and separators")
{
	CHECK(roleFromName("cam-2") == Role::Camera);
	CHECK(roleFromName("Écran.Keynote") == Role::Screen);
	CHECK(roleFromName("ecran keynote") == Role::Screen);
	CHECK(roleFromName("Caméra plateau") == Role::Camera);
	CHECK(roleFromName("Invité 3") == Role::Camera);
	CHECK(roleFromName("logo") == Role::Graphics);
	CHECK(roleFromName("LT_Valentin") == Role::Graphics);
	CHECK(roleFromName("Fond bleu") == Role::Background);
}

TEST("roles: a prefix must end at a separator")
{
	CHECK(roleFromName("Camping") == Role::None); /* not "CAM" */
	CHECK(roleFromName("BGM music") == Role::None);
	CHECK(roleFromName("Scratch") == Role::None);
	CHECK(roleFromName("Navigateur 2") == Role::None);
	CHECK(roleFromName("") == Role::None);
	CHECK(roleFromName("CAM") == Role::Camera);
}

TEST("roles: the longest prefix wins and custom rules apply")
{
	PrefixRules rules;
	rules.camera = {"PLATEAU"};
	rules.graphics = {"PLATEAU LOGO"};
	CHECK(roleFromName("Plateau 1", rules) == Role::Camera);
	CHECK(roleFromName("Plateau logo", rules) == Role::Graphics);
	CHECK(roleFromName("CAM 1", rules) == Role::None);
}

TEST("roles: keys and prefix lists")
{
	for (Role r :
	     {Role::None, Role::Host, Role::Camera, Role::Screen, Role::Background, Role::Graphics, Role::Chat})
		CHECK(roleFromKey(roleKey(r)) == r);
	CHECK(roleFromKey("bogus") == Role::None);
	const std::vector<std::string> l = splitPrefixList(" CAM, CAMÉRA ,,GUEST ");
	CHECK(l.size() == 3 && l[0] == "CAM" && l[1] == "CAMÉRA" && l[2] == "GUEST");
	CHECK(normalizeName("Œuvre Ça") == "OEUVRE CA");
}

TEST("roles: chat sources are recognized from their names")
{
	CHECK(roleFromName("CHAT _ YOUTUBE") == Role::Chat);
	CHECK(roleFromName("Tchat Twitch") == Role::Chat);
	CHECK(roleFromName("tchat-live") == Role::Chat);
	CHECK(roleFromName("ChatBox.StreamLabs") == Role::Chat);
	CHECK(roleFromName("CHATBOX") == Role::Chat);
	CHECK(roleFromName("chat") == Role::Chat);
	CHECK(roleFromName("Chatterbox") == Role::None); /* separator required */
	CHECK(roleFromName("Chatbot 2") == Role::None);
	CHECK(roleFromName("Tchatche") == Role::None);
	CHECK(!isCameraRole(Role::Chat));
	CHECK(roleKey(Role::Chat) == std::string("chat"));
	CHECK(roleFromKey("chat") == Role::Chat);
	CHECK((int)Role::Graphics == 5 && (int)Role::Chat == 6); /* saved values stay stable */
}
