/*
 * cefnetd Integration Example for FIB Active/Inactive Status
 * cefnetdでFIBアクティブ・インアクティブ機能を統合する例
 */

// 既存のInterest転送処理を拡張する例

/*--------------------------------------------------------------------------------------
	Interest転送処理の変更例
----------------------------------------------------------------------------------------*/
static int
cefnetd_incoming_interest_process_with_status (
    CefT_Netd_Handle* hdl,                  /* cefnetd handle */
    int faceid,                             /* Face-ID where messages arrived at */
    int peer_faceid,                        /* Face-ID to reply to the origin */
    unsigned char* msg,                     /* received message to handle */
    uint16_t payload_len,                   /* Payload Length of this message */
    uint16_t header_len,                    /* Header Length of this message */
    char* user_id
) {
    CefT_CcnMsg_MsgBdy pm = {0};
    CefT_CcnMsg_OptHdr poh = {0};
    CefT_Fib_Entry* fe = NULL;
    CefT_Pit_Entry* pe = NULL;
    uint16_t faceids[CefC_Fib_UpFace_Max];
    uint16_t name_len;
    int face_num = 0;
    int res;

    /* メッセージを解析 */
    res = cef_frame_message_parse (
        msg, payload_len, header_len, &poh, &pm, CefC_PT_INTEREST);
    if (res < 0) {
        return (-1);
    }

    /* Interest名の長さを決定 */
    if (pm.chunk_num_f) {
        name_len = pm.name_len - (CefC_S_Type + CefC_S_Length + CefC_S_ChunkNum);
    } else {
        name_len = pm.name_len;
    }

    /* **変更点**: アクティブなFIBエントリのみを検索 */
    fe = cef_fib_entry_search_active (hdl->fib, pm.name, name_len);

    if (fe) {
        /* FIBエントリが見つかった場合 */
        
        /* PITエントリを検索・更新 */
        pe = cef_pit_entry_lookup_and_down_face_update (
            hdl->pit, &pm, &poh, peer_faceid, msg, hdl->InterestRetrans, &res);

        if (pe) {
            /* **変更点**: アクティブなFace-IDのみを取得 */
            face_num = cef_fib_forward_faceid_get_active (fe, faceids);

            if (face_num > 0) {
                /* Interest転送 */
                cefnetd_interest_forward (
                    hdl, faceids, face_num, peer_faceid, msg,
                    payload_len, header_len, &pm, &poh, pe, fe);
                
#ifdef CefC_Debug
                cef_dbg_write (CefC_Dbg_Fine, 
                    "[Interest] Forwarded to %d active faces\n", face_num);
#endif
            } else {
                /* アクティブなFaceが見つからない場合のInterest Return */
                cef_log_write (CefC_Log_Warn, 
                    "No active faces found for Interest forwarding\n");
                
                /* Interest Returnを送信 */
                cefnetd_interest_return_send (hdl, peer_faceid, &pm, 
                    CefC_CtRc_NO_ROUTE, msg, payload_len, header_len);
            }
        }
    } else {
        /* アクティブなFIBエントリが見つからない場合 */
        cef_log_write (CefC_Log_Info, 
            "No active FIB entry found for Interest\n");
        
        /* Interest Returnを送信 */
        cefnetd_interest_return_send (hdl, peer_faceid, &pm, 
            CefC_CtRc_NO_ROUTE, msg, payload_len, header_len);
    }

    return (1);
}

/*--------------------------------------------------------------------------------------
	FIBエントリのステータス制御コマンド処理
----------------------------------------------------------------------------------------*/
static int
cefnetd_fib_status_control_command (
    CefT_Netd_Handle* hdl,                  /* cefnetd handle */
    unsigned char* msg,                     /* received message */
    int msg_size,                           /* message size */
    unsigned char** rsp_msgpp               /* response message */
) {
    unsigned char* rsp_msg = *rsp_msgpp;
    uint8_t operation;                      /* 操作種別 */
    uint8_t status;                         /* 設定するステータス */
    uint16_t name_len;                      /* 名前の長さ */
    unsigned char name[CefC_Max_Length];    /* 名前 */
    int index = 0;
    int res;

    /* 操作種別を取得 */
    if (msg_size < sizeof(operation)) {
        return (-1);
    }
    operation = msg[index];
    index += sizeof(operation);

    /* ステータスを取得 */
    if (msg_size - index < sizeof(status)) {
        return (-1);
    }
    status = msg[index];
    index += sizeof(status);

    /* 名前の長さを取得 */
    if (msg_size - index < sizeof(name_len)) {
        return (-1);
    }
    memcpy(&name_len, &msg[index], sizeof(name_len));
    index += sizeof(name_len);

    /* 名前を取得 */
    if (msg_size - index < name_len) {
        return (-1);
    }
    memcpy(name, &msg[index], name_len);

    /* 操作に応じて処理 */
    switch (operation) {
        case 0x01:  /* ステータス設定 */
            res = cef_fib_entry_status_set(hdl->fib, name, name_len, status);
            break;
            
        case 0x02:  /* ステータス取得 */
            status = cef_fib_entry_status_get(hdl->fib, name, name_len);
            res = 0;
            break;
            
        case 0x03:  /* 統計情報取得 */
            {
                int active_count, inactive_count, suspended_count;
                cef_fib_status_statistics_get(hdl->fib, 
                    &active_count, &inactive_count, &suspended_count);
                
                /* レスポンスメッセージを構築 */
                index = 0;
                rsp_msg[index++] = 0x01;  // 成功
                memcpy(&rsp_msg[index], &active_count, sizeof(int));
                index += sizeof(int);
                memcpy(&rsp_msg[index], &inactive_count, sizeof(int));
                index += sizeof(int);
                memcpy(&rsp_msg[index], &suspended_count, sizeof(int));
                index += sizeof(int);
                
                return (index);
            }
            break;
            
        case 0x04:  /* インアクティブエントリクリーンアップ */
            cef_fib_inactive_entries_cleanup(hdl->fib);
            res = 0;
            break;
            
        default:
            res = -1;
            break;
    }

    /* レスポンス */
    if (res == 0) {
        rsp_msg[0] = 0x01;  // 成功
        if (operation == 0x02) {  // ステータス取得の場合
            rsp_msg[1] = status;
            return (2);
        }
        return (1);
    } else {
        rsp_msg[0] = 0x00;  // 失敗
        return (1);
    }
}

/*--------------------------------------------------------------------------------------
	定期的なFIB自動管理（タイマー処理）
----------------------------------------------------------------------------------------*/
static void
cefnetd_fib_auto_management_timer (
    CefT_Netd_Handle* hdl                   /* cefnetd handle */
) {
    static uint64_t last_cleanup_time = 0;
    static uint64_t last_auto_mgmt_time = 0;
    uint64_t current_time;
    struct timespec ts;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    current_time = ts.tv_sec;
    
    /* 自動ステータス管理（5分間隔） */
    if (current_time - last_auto_mgmt_time > 300) {
        cef_fib_auto_status_management(hdl->fib, 
            30 * 60 * 1000000);  // 30分間未使用でインアクティブ
        last_auto_mgmt_time = current_time;
    }
    
    /* インアクティブエントリクリーンアップ（1時間間隔） */
    if (current_time - last_cleanup_time > 3600) {
        cef_fib_inactive_entries_cleanup(hdl->fib);
        last_cleanup_time = current_time;
    }
}

/*--------------------------------------------------------------------------------------
	設定ファイルでのFIBステータス初期化
----------------------------------------------------------------------------------------*/
static int
cefnetd_config_fib_status_read (
    CefT_Netd_Handle* hdl                   /* cefnetd handle */
) {
    FILE* fp = NULL;
    char file_path[CefC_Def_FilePath];
    char buff[CefC_BufSiz_1KB];
    char name[CefC_Max_Length];
    char status_str[16];
    uint8_t status;
    unsigned char name_tlv[CefC_Max_Length];
    int name_len;
    int res;

    /* 設定ファイルパスを構築 */
    sprintf(file_path, "%s/cefnetd.fib_status", hdl->config_dir);
    
    fp = fopen(file_path, "r");
    if (fp == NULL) {
        /* 設定ファイルが存在しない場合はスキップ */
        return (0);
    }

    /* 設定ファイルを読み込み */
    while (fgets(buff, sizeof(buff), fp) != NULL) {
        if (buff[0] == '#' || buff[0] == '\n') {
            continue;  // コメント行または空行をスキップ
        }

        /* 行を解析：<name> <status> */
        res = sscanf(buff, "%s %s", name, status_str);
        if (res != 2) {
            continue;
        }

        /* ステータス文字列を数値に変換 */
        if (strcmp(status_str, "active") == 0) {
            status = CefC_Fib_Status_Active;
        } else if (strcmp(status_str, "inactive") == 0) {
            status = CefC_Fib_Status_Inactive;
        } else if (strcmp(status_str, "suspended") == 0) {
            status = CefC_Fib_Status_Suspended;
        } else {
            continue;  // 未知のステータス
        }

        /* URIをTLV名前に変換 */
        name_len = cef_frame_conversion_uri_to_name(name, name_tlv);
        if (name_len < 0) {
            continue;
        }

        /* FIBエントリのステータスを設定 */
        cef_fib_entry_status_set(hdl->fib, name_tlv, name_len, status);

#ifdef CefC_Debug
        cef_dbg_write(CefC_Dbg_Fine, 
            "[Config] Set FIB entry %s to %s\n", name, status_str);
#endif
    }

    fclose(fp);
    return (1);
} 