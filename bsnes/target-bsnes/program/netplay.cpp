auto Program::netplayStart() -> void {
    gekko_create(&netplay.session);
    showMessage("Netplay started\n");
}