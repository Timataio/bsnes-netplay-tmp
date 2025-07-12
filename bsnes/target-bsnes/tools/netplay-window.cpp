auto NetplayWindow::create() -> void {
    layout.setPadding(8_sx);

    portLabel.setText("Local Port:");
    portValue.onChange([&] { config.localPort = portValue.text().strip().integer();});

    rollbackframeLabel.setText({"Max Rollback: ", config.rollbackframes});
    rollbackframeValue.setPosition(config.rollbackframes);
    rollbackframeValue.setLength(11).onChange([&] { 
        config.rollbackframes = rollbackframeValue.position();
        rollbackframeLabel.setText({"Max Rollback: ", config.rollbackframes}); 
    });

    delayLabel.setText({"Delay: ", config.localDelay});
    delayValue.setPosition(config.localDelay);
    delayValue.setLength(16).onChange([&] { 
        config.localDelay = delayValue.position();
        delayLabel.setText({"Delay: ", config.localDelay}); 
    });

    enableMultitap.setText("Enable Multitap").onToggle([&] {
        config.multitap = enableMultitap.checked();
        updateMultitap();
    });

    specPlayerCountLabel.setText("Player Count:");
    specPlayerCountValue.onChange([&] { config.spectatorPlayerCount = specPlayerCountValue.text().strip().integer();}).setEnabled(false);

    playerLabel.setText("Local Controller:");   
    specSelect.setText("None").onActivate([&] { updateLocalController(255); });
    p1Select.setText("P1").onActivate([&] { updateLocalController(0); });
    p2Select.setText("P2").onActivate([&] { updateLocalController(1); });
    p3Select.setText("P3").onActivate([&] { updateLocalController(2); }).setEnabled(false);
    p4Select.setText("P4").onActivate([&] { updateLocalController(3); }).setEnabled(false);
    p5Select.setText("P5").onActivate([&] { updateLocalController(4); }).setEnabled(false);

    remoteLabel
        .setText("Remote Player Addresses:")
        .setToolTip("Comma-separated list of IP:PORT pairs in ascending order of assigned controller port number.");

    remoteValue.onChange([&] { 
        config.remotes = remoteValue.text().strip().split(",").strip();
        config.remotes = filterAddresses(config.remotes);
        updateMultitap();
    });

    spectatorLabel
        .setText("Spectator Addresses:")
        .setToolTip("Comma-separated list of IP:PORT pairs, order does not matter.");

    spectatorValue.onChange([&] { 
        config.spectators = spectatorValue.text().strip().split(",").strip();
        config.spectators = filterAddresses(config.spectators);
    });

    if(config.localPlayer == 255) specSelect.setChecked();
    if(config.localPlayer == 0) p1Select.setChecked();
    if(config.localPlayer == 1) p2Select.setChecked();
    if(config.localPlayer == 2) p3Select.setChecked();
    if(config.localPlayer == 3) p4Select.setChecked();
    if(config.localPlayer == 4) p5Select.setChecked();

    btnStart.setText("Start Session").setIcon(Icon::Media::Play).onActivate([&] {
        if(portValue.text().strip().size() == 0) {
            MessageDialog("Local port number is required")
            .setTitle("Netplay Setup Error")
            .error();
            return;
        }
        if(config.remotes.size() == 0) {
            MessageDialog("Remote player addresses are required")
            .setTitle("Netplay Setup Error")
            .error();
            return;
        }
        if(!config.multitap && config.remotes.size() > 1 && config.localPlayer != 255) {
            MessageDialog("Multitap mode is not enabled, but more than one remote player has been specified.")
            .setTitle("Netplay Setup Error")
            .error();
            return;
        }
        if(config.localPlayer == 255 && config.remotes.size() > 1) {
            MessageDialog("Only one remote player address is needed when spectating.")
            .setTitle("Netplay Setup Error")
            .error();
            return;
        }
        if(config.localPlayer == 255 && specPlayerCountValue.text().strip().integer() == 0) {
            MessageDialog("Player count is required when spectating.\nUse 2 for 2 players playing, 3 for 3 players playing, etc.")
            .setTitle("Netplay Setup Error")
            .error();
            return;
        }

        // fill the remotes with fillers so the session knows how many players there are
        if(config.localPlayer == 255) {
            // -1 cuz the first remote in the list is the spectator's host.
            for(int i = 0; i < config.spectatorPlayerCount - 1; i++) {
                config.remotes.append({"SPEC_FILLER"});
            } 
        }
        
        program.netplayStart(config.localPort, config.localPlayer, config.rollbackframes, config.localDelay, config.remotes, config.spectators);
        doClose(); 
    });
    btnCancel.setText("Cancel").setIcon(Icon::Action::Quit).onActivate([&] { doClose(); });

    setTitle("Netplay Setup");
    setSize({400_sx, 300_sy});
    setResizable(false);
    setAlignment({0.5, 0.5});

    onClose([&] {
        setVisible(false);
    });
}

auto NetplayWindow::setVisible(bool visible) -> NetplayWindow& {
    if(visible) {
        Application::processEvents();
    }
    return Window::setVisible(visible), *this;
}

auto NetplayWindow::show() -> void {
    setVisible();
    setFocused();
}

auto NetplayWindow::updateMultitap() -> void {
    if(config.remotes.size() > 1) p3Select.setEnabled(config.multitap); else p3Select.setEnabled(false);
    if(config.remotes.size() > 2) p4Select.setEnabled(config.multitap); else p4Select.setEnabled(false);
    if(config.remotes.size() > 3) p5Select.setEnabled(config.multitap); else p5Select.setEnabled(false);
}

auto NetplayWindow::updateLocalController(uint8 port) -> void {
    const bool isPlayer = port < 5;
    config.localPlayer = isPlayer ? port : 255;
    spectatorValue.setEnabled(isPlayer);
    enableMultitap.setEnabled(isPlayer);
    if(!isPlayer) enableMultitap.setChecked(false);
    specPlayerCountValue.setEnabled(!isPlayer);
}

auto NetplayWindow::filterAddresses(vector<string>& addresses) -> vector<string> {
    vector<string> tmp;
    for(int i = 0; i < addresses.size(); i++)
        if(addresses[i].size() > 0) tmp.append(addresses[i]);
    return tmp;
}