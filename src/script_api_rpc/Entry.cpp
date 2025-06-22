#include "Entry.h"
#include "Config.h"
#include <mc/world/actor/state/PropertySyncData.h>


#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/io/LoggerRegistry.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/service/ServerInfo.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/NetEventCallback.h"
#include "mc/network/NetworkSystem.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/world/actor/Actor.h"
#include <fmt/format.h>
#include <ll/api/Config.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/data/KeyValueDB.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/io/FileUtils.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/mod/ModManagerRegistry.h>
#include <ll/api/mod/NativeMod.h>
#include <ll/api/service/Bedrock.h>
#include <mc/entity/components/SynchedActorDataComponent.h>
#include <mc/network/MinecraftPackets.h>
#include <mc/network/PacketSender.h>
#include <mc/network/packet/AddActorPacket.h>
#include <mc/network/packet/BookEditPacket.h>
#include <mc/network/packet/InventoryContentPacket.h>
#include <mc/network/packet/LegacyTelemetryEventPacket.h>
#include <mc/network/packet/LoginPacket.h>
#include <mc/network/packet/SetActorDataPacket.h>
#include <mc/network/packet/SyncedAttribute.h>
#include <mc/scripting/ServerScriptManager.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/world/actor/ActorDataIDs.h>
#include <mc/world/actor/ActorLink.h>
#include <mc/world/actor/DataItem.h>
#include <mc/world/actor/DataItem2.h>
#include <mc/world/actor/DataItemType.h>
#include <mc/world/actor/SynchedActorData.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/state/PropertySyncData.h>
#include <mc/world/attribute/AttributeInstanceHandle.h>
#include <mc/world/item/NetworkItemStackDescriptor.h>
#include <memory>
#include <stdexcept>


#include <string>


// Bossbar: https://github.com/GroupMountain/GMLIB-Release/blob/develop/src/Server/PlayerAPI.cc#L485-L566

auto dedicatedServerLogger = ll::io::LoggerRegistry::getInstance().getOrCreate("DedicatedServer");

void translateEntityNameTag(std::vector<std::unique_ptr<DataItem>>& mData, Player* player) {
    auto it = std::find_if(mData.begin(), mData.end(), [&](const std::unique_ptr<DataItem>& item) {
        return item && item->getId() == (ushort)ActorDataIDs::Name;
    });

    if (it != mData.end()) {
        size_t index   = std::distance(mData.begin(), it);
        (mData)[index] = DataItem::create(
            ActorDataIDs::Name,
            std::string("Custom entity name for " + player->getRealName() + " " + player->getLocaleCode())
        );
    } else {
        dedicatedServerLogger->info("Not found name tag for data with size {}", mData.size());
    }
}


LL_AUTO_TYPE_INSTANCE_HOOK(
    PacketSenderHook,
    ll::memory::HookPriority::High,
    NetworkSystem,
    &NetworkSystem::send,
    void,
    const ::NetworkIdentifier& id,
    const Packet&              packet,
    ::SubClientId              subClientId
) {
    auto serverNetworkHandler = ll::service::getServerNetworkHandler();
    if (!serverNetworkHandler) {
        return origin(id, packet, subClientId);
    }

    ServerPlayer* player = serverNetworkHandler->_getServerPlayer(id, subClientId);
    if (!player) {
        return origin(id, packet, subClientId);
    }

    auto packetId = packet.getId();
    dedicatedServerLogger->info("Sending {} to {}", packet.getName(), player->getRealName());

    if (packetId == MinecraftPacketIds::AddActor || packetId == MinecraftPacketIds::LegacyTelemetryEvent
        || packetId == MinecraftPacketIds::CommandOutput) {
        BinaryStream bs;
        packet.write(bs);
        dedicatedServerLogger->info("Text data {}", bs.mBuffer.data());
    }

    if (packetId == MinecraftPacketIds::SetActorData || packetId == MinecraftPacketIds::AddActor
        || packetId == MinecraftPacketIds::InventoryContent || packetId == MinecraftPacketIds::ActorEvent
        || packetId == MinecraftPacketIds::SyncActorProperty) {


        if (packetId == MinecraftPacketIds::SetActorData) {
            auto& casted = static_cast<const SetActorDataPacket&>(packet);

            // BinaryStream bs;
            // packet.write(bs);
            // SetActorDataPacket newPacket{casted.mId, casted.mPackedItems, casted.mSynchedProperties, casted.mTick,
            // false};

            // BinaryStream bs2;
            // newPacket.write(bs2);
            // dedicatedServerLogger->info("Text data {}", bs2.mBuffer.data());

            // newPacket._read(bs);

            for (auto& item : casted.mPackedItems) {
                if (item) {
                    auto itemDataId = item->getId();
                    if (itemDataId == (ushort)ActorDataIDs::Name) {
                        item->setData<std::string>("Translate and set name here");
                        dedicatedServerLogger->info(
                            "Text nameTag data with content {}",
                            item->getData<std::string>()->data()
                        );
                    }
                }
            }

            dedicatedServerLogger->info("Data size {}", casted.mPackedItems.size());

            return origin(id, casted, subClientId);
        }

        // if (packetId == MinecraftPacketIds::AddActor) {
        // BinaryStream bs;
        // packet.write(bs);
        // AddActorPacket newPacket{};
        // newPacket.read(bs);

        //     for (auto& item : *newPacket.mData) {
        //         if (item) {
        //             auto itemDataId = item->getId();
        //             if (itemDataId == (ushort)ActorDataIDs::Name) {
        //                 // item->setData<std::string>("Custom name HAAH LL");
        //                 dedicatedServerLogger->info(
        //                     "Text nameTag data with id {} and content {}",
        //                     itemDataId,
        //                     item->getData<std::string>()->data()
        //                 );
        //             }
        //         }
        //     }

        //     BinaryStream bs2;
        //     newPacket.write(bs2);

        //     dedicatedServerLogger->info("Buffer1 '{}' '{}' ", bs.mBuffer.data(), bs2.mBuffer.data());

        //     return origin(id, packet, subClientId);
        // }

        if (packetId == MinecraftPacketIds::AddActor) {
            auto& casted = static_cast<const AddActorPacket&>(packet);
            auto& mData  = casted.mEntityData->mData.get()->mData->mItemsArray;

            translateEntityNameTag(mData, player);

            return origin(id, casted, subClientId);
        }

        // if (packetId == MinecraftPacketIds::BookEdit) {
        //     BookEditPacket actorData(static_cast<const BookEditPacket&>(packet));
        //     dedicatedServerLogger->info("Got properties");
        // }


        // if (packetId == MinecraftPacketIds::Login) {
        //     LoginPacket actorData(static_cast<const LoginPacket&>(packet));
        //     dedicatedServerLogger->info("Got properties");
        // }

        // if (packetId == MinecraftPacketIds::InventoryContent) {
        //     InventoryContentPacket casted = (static_cast<const InventoryContentPacket&>(packet));
        //     InventoryContentPacket copiedAndCasted(casted);
        //     // Modify inventory content
        //     return origin(id, copiedAndCasted, subClientId);
        // }
    }
    return origin(id, packet, subClientId);
}


namespace {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

script_api_rpc::Config                config;
std::unique_ptr<ll::data::KeyValueDB> playerDb;

// Event listeners
ll::event::ListenerPtr playerJoinEventListener;
ll::event::ListenerPtr playerLeaveEventListener;


// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace

namespace script_api_rpc {

Mod& Mod::getInstance() {
    static Mod instance;
    return instance;
}

bool Mod::load() const {
    getSelf().getLogger().info("Loading...");

    auto& logger = getSelf().getLogger();

    // Load or initialize configurations.
    const auto& configFilePath = getSelf().getConfigDir() / "config.json";
    if (!ll::config::loadConfig(config, configFilePath)) {
        logger.warn("Cannot load configurations from {}", configFilePath);
        logger.info("Saving default configurations");

        if (!ll::config::saveConfig(config, configFilePath)) {
            logger.error("Cannot save default configurations to {}", configFilePath);
        }
    }

    // Initialize databases;
    const auto& playerDbPath = getSelf().getDataDir() / "players";
    playerDb                 = std::make_unique<ll::data::KeyValueDB>(playerDbPath);


    // Code for loading the mod goes here.
    return true;
}

struct PlayerMapValue {
    std::string uuid;
    std::string xuid;
    std::string locale;
};
std::map<std::string, nlohmann::json> playersMap;

void Mod::writeMapToFile() const {
    const auto&    mapFilePath = getSelf().getConfigDir() / "current-players.json";
    nlohmann::json jsonObject  = playersMap;
    getSelf().getLogger().info("Saving JSON: {}", jsonObject.dump());
    ll::file_utils::writeFile(mapFilePath, jsonObject.dump());
};


bool Mod::enable() const {
    auto& logger = getSelf().getLogger();

    logger.info("Enabling...");

    if (!ll::setServerMotd("Custom motd")) {
        logger.info("Unable to set motd");
    }


    // Register commands.
    auto commandRegistry = ll::service::getCommandRegistry();
    if (!commandRegistry) {
        throw std::runtime_error("failed to get command registry");
    }
    if (config.registerSuicideCommand) {
        auto& command = ll::command::CommandRegistrar::getInstance().getOrCreateCommand(
            "customcommandsuicide",
            "Commits suicide.",
            CommandPermissionLevel::Any
        );


        command.overload().execute([&logger](CommandOrigin const& origin, CommandOutput& output) {
            auto* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player)) {
                return output.error("Only players can commit suicide");
            }


            auto* player = static_cast<Player*>(entity); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            player->kill();

            logger.info("{} killed themselves", player->getRealName());
        });
    }


    auto& eventBus = ll::event::EventBus::getInstance();


    playerJoinEventListener = eventBus.emplaceListener<ll::event::player::PlayerJoinEvent>(
        [&logger, &playersMap = playersMap, this](ll::event::player::PlayerJoinEvent& event) {
            auto& player = event.self();
            auto& uuid   = player.getUuid();
            logger.info("[LangCodeMapp] '{}' with '{}' joined", player.getRealName(), player.getLocaleCode());

            auto value      = nlohmann::json::object();
            value["uuid"]   = uuid.asString();
            value["xuid"]   = player.getXuid();
            value["locale"] = player.getLocaleCode();
            playersMap.insert_or_assign(uuid.asString(), value);
            writeMapToFile();
        }
    );

    playerLeaveEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>(
        [&logger, &playersMap = playersMap, this](ll::event::player::PlayerDisconnectEvent& event) {
            auto& player = event.self();
            auto  uuid   = player.getUuid().asString();
            logger.info("{} Left", player.getRealName());

            playersMap.erase(uuid);
            writeMapToFile();
        }
    );

    return true;
}

bool Mod::disable() const {
    getSelf().getLogger().debug("Disabling...");

    return true;
}

bool Mod::unload() const {
    getSelf().getLogger().debug("Unloading...");

    return true;
}


} // namespace script_api_rpc

LL_REGISTER_MOD(script_api_rpc::Mod, script_api_rpc::Mod::getInstance());
