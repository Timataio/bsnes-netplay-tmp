static uint16 testIps[] = {1111, 2222, 8331, 8332, 8333, 8334, 8335, 8336};
auto Program::netplayStart(uint8 numPlayers, uint8 local) -> void {
    if(netplay.mode != Netplay::Mode::Inactive || numPlayers == 0)
        return;

    netplayMode(Netplay::Setup);

    netplay.peers.reset();
    netplay.inputs.reset();
    netplay.states.reset();

    for(int i = 0; i < numPlayers; i++) {
        netplay.inputs.append(Netplay::Buttons());
    }

    netplay.config.num_players = numPlayers;
    netplay.config.input_size = sizeof(Netplay::Buttons);
    netplay.config.state_size = sizeof(int32);
    netplay.config.max_spectators = 6;
    netplay.config.input_prediction_window = 10;
    netplay.config.spectator_delay = 90;

    netplay.states.resize(netplay.config.input_prediction_window + 2);

    gekko_create(&netplay.session);
    gekko_start(netplay.session, &netplay.config);
    gekko_net_adapter_set(netplay.session, gekko_default_adapter(testIps[local]));

    if(local == 0) {
        auto peer1 = Netplay::Peer();
        auto peer2 = Netplay::Peer();

        peer1.id = gekko_add_actor(netplay.session, LocalPlayer, nullptr);
        peer1.nickname = "P1";
        peer1.conn.ip = "localhost";
        peer1.conn.port = testIps[0];

        peer2.nickname = "P2";
        peer2.conn.ip = "127.0.0.1";
        peer2.conn.port = testIps[1];
        
        auto remAddr = peer2.conn.toString();
        auto remote = GekkoNetAddress{ (void*)remAddr.data(), remAddr.size() };
        peer2.id = gekko_add_actor(netplay.session, RemotePlayer, &remote);

        netplay.peers.append(peer1);
        netplay.peers.append(peer2);

        // add the expected spectators
        for(int i = 0; i < netplay.config.max_spectators; i++) {
            auto specPeer = Netplay::Peer();
            
            specPeer.nickname = "Spectator";
            specPeer.conn.ip = "127.0.0.1";
            specPeer.conn.port = testIps[2 + i];
        
            auto specAddr = specPeer.conn.toString();
            auto spec = GekkoNetAddress{ (void*)specAddr.data(), specAddr.size() };
            specPeer.id = gekko_add_actor(netplay.session, Spectator, &spec);

            netplay.peers.append(specPeer);
        }
    } else if (local == 1){
        auto peer1 = Netplay::Peer();
        auto peer2 = Netplay::Peer();

        peer1.nickname = "P1";
        peer1.conn.ip = "127.0.0.1";
        peer1.conn.port = testIps[0];

        auto remAddr = peer1.conn.toString();
        auto remote = GekkoNetAddress{ (void *)remAddr.data(), remAddr.size() };
        peer1.id = gekko_add_actor(netplay.session, RemotePlayer, &remote);

        peer2.id = gekko_add_actor(netplay.session, LocalPlayer, nullptr);
        peer2.nickname = "P2";
        peer2.conn.ip = "localhost";
        peer2.conn.port = testIps[1];

        netplay.peers.append(peer1);
        netplay.peers.append(peer2);
    }else{
        auto peer1 = Netplay::Peer();

        peer1.nickname = "P1";
        peer1.conn.ip = "127.0.0.1";
        peer1.conn.port = testIps[0];

        auto remAddr = peer1.conn.toString();
        auto remote = GekkoNetAddress{ (void *)remAddr.data(), remAddr.size() };
        peer1.id = gekko_add_actor(netplay.session, RemotePlayer, &remote);

        netplay.peers.append(peer1);
    }

    for(int i = 0; i < netplay.peers.size(); i++) {
        print("P:", netplay.peers[i].conn.toString(), "\n");
    }

    netplayMode(Netplay::Running);
}

auto Program::netplayMode(Netplay::Mode mode) -> void {
    if(netplay.mode == mode) return;
    if(mode == Netplay::Running) {
        inputSettings.blockInput.setChecked();
        emulator->configure("Hacks/Entropy", "None");
        if(emulator->loaded()) emulator->power();
    }
    netplay.mode = mode;
}

auto Program::netplayStop() -> void {
    if (netplay.mode == Netplay::Mode::Inactive)
        return;

    netplayMode(Netplay::Inactive);
    gekko_destroy(netplay.session);

    netplay.session = nullptr;
    netplay.config = {};

    netplay.counter = 0;
}

auto Program::netplayRun() -> bool {
    if(netplay.session) gekko_network_poll(netplay.session);
    if (netplay.mode != Netplay::Mode::Running) return false;

    netplay.counter++;

    if(gekko_frames_ahead(netplay.session) >= 1.5f && netplay.counter % 60 == 0) {
        // rift syncing first attempt
        // kinda hacky. there's prolly a better way but im clueless.
        netplayHaltFrame();
        return true;
    }

    for(int i = 0; i < netplay.peers.size(); i++) {
        if(netplay.peers[i].conn.ip != "localhost") continue;
        Netplay::Buttons input = {};
        netplayPollLocalInput(input);
        gekko_add_local_input(netplay.session, netplay.peers[i].id, &input);
    }
    
    int count = 0;
    auto events = gekko_session_events(netplay.session, &count);
    for(int i = 0; i < count; i++) {
        auto event = events[i];
        int type = event->type;
        print("EV:", type, "\n");
        if (event->type == PlayerDisconnected) {
            auto disco = event->data.disconnected;
            print("Disconnect detected, player:", disco.handle, "\n");
        }
    }

    count = 0;
    auto updates = gekko_update_session(netplay.session, &count);
    for (int i = 0; i < count; i++) {
        auto ev = updates[i];
        int frame = 0, cframe = 0;
        auto serial = serializer();

        switch (ev->type) {
        case SaveEvent:
            // save the state ourselves
            serial = emulator->serialize(0);
            frame = ev->data.save.frame % netplay.states.size();
            netplay.states[frame].set(serial.data(), serial.size());
            // print("Saved frame:", ev->data.save.frame," mod:", frame, " size:", serial.size(),"\n" );
            // pass the frame number so we can later use it to get the right state
            *ev->data.save.checksum = 0; // maybe can be helpful later.
            *ev->data.save.state_len = sizeof(int32);
            memcpy(ev->data.save.state, &ev->data.save.frame, sizeof(int32));
            break;
        case LoadEvent:
            frame = ev->data.load.frame % netplay.states.size();
            serial = serializer(netplay.states[frame].data(), netplay.states[frame].size());
            emulator->unserialize(serial);
            program.mute |= Mute::Always;
            emulator->setRunAhead(true);
            break;
        case AdvanceEvent:
            if(emulator->runAhead()) {
                cframe = count - 1;
                if(cframe == i || (updates[cframe]->type == SaveEvent && i == cframe - 1)) {
                    emulator->setRunAhead(false);
                    program.mute &= ~Mute::Always;
                }
            }
            memcpy(netplay.inputs.data(), ev->data.adv.inputs, sizeof(Netplay::Buttons) * netplay.config.num_players);
            emulator->run();
            break;
        }
    }
    return true;
}
auto Program::netplayPollLocalInput(Netplay::Buttons &localInput) -> void {
    localInput.u.value = 0;
    if (focused() || inputSettings.allowInput().checked()) {
        inputManager.poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::B)) localInput.u.btn.b = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::Y)) localInput.u.btn.y = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::Select)) localInput.u.btn.select = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::Start)) localInput.u.btn.start = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::Up)) localInput.u.btn.up = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::Down)) localInput.u.btn.down = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::Left)) localInput.u.btn.left = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::Right)) localInput.u.btn.right = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::A)) localInput.u.btn.a = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::X)) localInput.u.btn.x = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::L)) localInput.u.btn.l = mapping->poll();
        if (auto mapping = inputManager.mapping(0, 1, Netplay::SnesButton::R)) localInput.u.btn.r = mapping->poll();
    }
}

auto Program::netplayGetInput(uint port, uint button) -> int16 {
    switch (button) {
    case Netplay::SnesButton::B: return netplay.inputs[port].u.btn.b;
    case Netplay::SnesButton::Y: return netplay.inputs[port].u.btn.y;
    case Netplay::SnesButton::Select: return netplay.inputs[port].u.btn.select;
    case Netplay::SnesButton::Start: return netplay.inputs[port].u.btn.start;
    case Netplay::SnesButton::Up: return netplay.inputs[port].u.btn.up;
    case Netplay::SnesButton::Down: return netplay.inputs[port].u.btn.down;
    case Netplay::SnesButton::Left: return netplay.inputs[port].u.btn.left;
    case Netplay::SnesButton::Right: return netplay.inputs[port].u.btn.right;
    case Netplay::SnesButton::A: return netplay.inputs[port].u.btn.a;
    case Netplay::SnesButton::X: return netplay.inputs[port].u.btn.x;
    case Netplay::SnesButton::L: return netplay.inputs[port].u.btn.l;
    case Netplay::SnesButton::R: return netplay.inputs[port].u.btn.r;
    default:
        return 0;
    }
}

auto Program::netplayHaltFrame() -> void
{
    auto state = emulator->serialize(0);
    emulator->run();
    state.setMode(serializer::Mode::Load);
    emulator->unserialize(state);
}
