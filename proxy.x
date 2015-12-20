program PROXY {
    version PROXY_VERSION {
        string GET_PID_LIST(int) = 1;
        int COMFIRM(int) = 2;
    } = 1;
} = 0x12345678;
