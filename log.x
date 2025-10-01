struct log_msg {
    string user_log<256>;
    string op_log<16>;
    string file_name_log<256>;
    string time_log<32>;
    string date_log<32>;
};

program LOG {
    version LOG_VER {
        int send_log(struct log_msg log) = 1;
    } = 1;
} = 100495821;