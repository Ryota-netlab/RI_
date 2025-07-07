/*
 * FIB Active/Inactive Status Extension Example
 * CeforeのFIBにアクティブ・インアクティブ機能を追加する実装例
 */

#ifndef __CEF_FIB_STATUS_EXT_H__
#define __CEF_FIB_STATUS_EXT_H__

/****************************************************************************************
 新しいステータス定義
 ****************************************************************************************/

/* FIBエントリのステータス */
#define CefC_Fib_Status_Active      0x01    /* アクティブ状態 */
#define CefC_Fib_Status_Inactive    0x02    /* インアクティブ状態 */
#define CefC_Fib_Status_Suspended   0x04    /* 一時停止状態 */

/* Face単位のステータス */
#define CefC_Face_Status_Active     0x01    /* Face アクティブ */
#define CefC_Face_Status_Inactive   0x02    /* Face インアクティブ */

/****************************************************************************************
 拡張されたFIB構造体（例）
 ****************************************************************************************/

/* 方法1: FIBエントリ全体のステータス管理 */
typedef struct {
    unsigned char* 	key;
    unsigned int 	klen;
    CefT_Fib_Face	faces;
    uint64_t		rx_int;
    uint64_t		rx_int_types[CefC_PIT_TYPE_MAX];
    
    /* 新規追加フィールド */
    uint8_t         status;         /* エントリのアクティブ・インアクティブ状態 */
    uint64_t        last_used;      /* 最後に使用された時刻（自動制御用） */
    uint32_t        priority;       /* 優先度（複数エントリ競合時用） */
    
    uint16_t 		app_comp;
    uint64_t 		lifetime;
} CefT_Fib_Entry_Extended;

/* 方法2: Face単位のステータス管理 */
typedef struct CefT_Fib_Face_Extended {
    int 	faceid;
    struct CefT_Fib_Face_Extended* next;
    int 	type;
    
    /* 新規追加フィールド */
    uint8_t status;                 /* Face のアクティブ・インアクティブ状態 */
    uint64_t last_active_time;      /* 最後にアクティブだった時刻 */
    
    CefT_Fib_Metric	metric;
    uint64_t		tx_int;
    uint64_t		tx_int_types[CefC_PIT_TYPE_MAX];
} CefT_Fib_Face_Extended;

/****************************************************************************************
 新しい管理関数の宣言
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
	FIBエントリのステータスを設定
----------------------------------------------------------------------------------------*/
int
cef_fib_entry_status_set (
    CefT_Hash_Handle fib,               /* FIB */
    unsigned char* name,                /* エントリ名 */
    uint16_t name_len,                  /* エントリ名長 */
    uint8_t status                      /* 設定するステータス */
);

/*--------------------------------------------------------------------------------------
	FIBエントリのステータスを取得
----------------------------------------------------------------------------------------*/
uint8_t
cef_fib_entry_status_get (
    CefT_Hash_Handle fib,               /* FIB */
    unsigned char* name,                /* エントリ名 */
    uint16_t name_len                   /* エントリ名長 */
);

/*--------------------------------------------------------------------------------------
	アクティブなFIBエントリのみを検索（既存の検索関数を拡張）
----------------------------------------------------------------------------------------*/
CefT_Fib_Entry*
cef_fib_entry_search_active (
    CefT_Hash_Handle fib,               /* FIB */
    unsigned char* name,                /* エントリ名 */
    uint16_t name_len                   /* エントリ名長 */
);

/*--------------------------------------------------------------------------------------
	アクティブなFace-IDのみを取得（既存の取得関数を拡張）
----------------------------------------------------------------------------------------*/
int
cef_fib_forward_faceid_get_active (
    CefT_Fib_Entry* entry,              /* FIBエントリ */
    uint16_t faceids[]                  /* アクティブなFace-IDの配列 */
);

/*--------------------------------------------------------------------------------------
	Face単位でのステータス制御
----------------------------------------------------------------------------------------*/
int
cef_fib_face_status_set (
    CefT_Fib_Entry* entry,              /* FIBエントリ */
    uint16_t faceid,                    /* 対象Face-ID */
    uint8_t status                      /* 設定するステータス */
);

/*--------------------------------------------------------------------------------------
	すべてのインアクティブエントリをクリーンアップ
----------------------------------------------------------------------------------------*/
void
cef_fib_inactive_entries_cleanup (
    CefT_Hash_Handle fib                /* FIB */
);

/*--------------------------------------------------------------------------------------
	統計情報：アクティブ・インアクティブエントリ数を取得
----------------------------------------------------------------------------------------*/
void
cef_fib_status_statistics_get (
    CefT_Hash_Handle fib,               /* FIB */
    int* active_count,                  /* アクティブエントリ数 */
    int* inactive_count,                /* インアクティブエントリ数 */
    int* suspended_count                /* 一時停止エントリ数 */
);

#endif // __CEF_FIB_STATUS_EXT_H__ 