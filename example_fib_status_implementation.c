/*
 * FIB Active/Inactive Status Extension Implementation Example
 * CeforeのFIBにアクティブ・インアクティブ機能を追加する実装例
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// 既存のCeforeヘッダーファイル（仮想的なインクルード）
// #include <cefore/cef_fib.h>
// #include "example_fib_status_extension.h"

/****************************************************************************************
 FIBエントリのステータス管理実装
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
) {
    CefT_Fib_Entry_Extended* entry;
    
    /* エントリを検索 */
    entry = (CefT_Fib_Entry_Extended*) cef_hash_tbl_item_get(fib, name, name_len);
    if (entry == NULL) {
        return (-1);  // エントリが見つからない
    }
    
    /* ステータスを設定 */
    entry->status = status;
    
    /* 最終使用時刻を更新 */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    entry->last_used = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    
#ifdef CefC_Debug
    cef_dbg_write(CefC_Dbg_Fine, 
        "[FIB] Entry status set to %s for name_len=%d\n",
        (status == CefC_Fib_Status_Active) ? "ACTIVE" : 
        (status == CefC_Fib_Status_Inactive) ? "INACTIVE" : "SUSPENDED",
        name_len);
#endif
    
    return (0);
}

/*--------------------------------------------------------------------------------------
	FIBエントリのステータスを取得
----------------------------------------------------------------------------------------*/
uint8_t
cef_fib_entry_status_get (
    CefT_Hash_Handle fib,               /* FIB */
    unsigned char* name,                /* エントリ名 */
    uint16_t name_len                   /* エントリ名長 */
) {
    CefT_Fib_Entry_Extended* entry;
    
    /* エントリを検索 */
    entry = (CefT_Fib_Entry_Extended*) cef_hash_tbl_item_get(fib, name, name_len);
    if (entry == NULL) {
        return (CefC_Fib_Status_Inactive);  // 見つからない場合はインアクティブとして扱う
    }
    
    return (entry->status);
}

/*--------------------------------------------------------------------------------------
	アクティブなFIBエントリのみを検索（既存の検索関数を拡張）
----------------------------------------------------------------------------------------*/
CefT_Fib_Entry*
cef_fib_entry_search_active (
    CefT_Hash_Handle fib,               /* FIB */
    unsigned char* name,                /* エントリ名 */
    uint16_t name_len                   /* エントリ名長 */
) {
    CefT_Fib_Entry_Extended* entry;
    unsigned char* msp;
    unsigned char* mep;
    uint16_t len = name_len;
    uint16_t length;
    
    /* 最長プレフィックスマッチでアクティブなエントリを検索 */
    while (len > 0) {
        entry = (CefT_Fib_Entry_Extended*) cef_hash_tbl_item_get(fib, name, len);
        
        if (entry != NULL && entry->status == CefC_Fib_Status_Active) {
#ifdef CefC_Debug
            cef_dbg_write(CefC_Dbg_Finest, 
                "[FIB] Found active entry with name_len=%d\n", len);
#endif
            /* 最終使用時刻を更新 */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            entry->last_used = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
            
            return ((CefT_Fib_Entry*)entry);
        }
        
        /* プレフィックスを短縮して再検索 */
        msp = name;
        mep = name + len - 1;
        while (msp < mep) {
            memcpy(&length, &msp[CefC_S_Length], CefC_S_Length);
            length = ntohs(length);
            
            if (msp + CefC_S_Type + CefC_S_Length + length < mep) {
                msp += CefC_S_Type + CefC_S_Length + length;
            } else {
                break;
            }
        }
        len = msp - name;
    }
    
    return (NULL);  // アクティブなエントリが見つからない
}

/*--------------------------------------------------------------------------------------
	アクティブなFace-IDのみを取得
----------------------------------------------------------------------------------------*/
int
cef_fib_forward_faceid_get_active (
    CefT_Fib_Entry* entry,              /* FIBエントリ */
    uint16_t faceids[]                  /* アクティブなFace-IDの配列 */
) {
    int i = 0;
    CefT_Fib_Face_Extended* face = (CefT_Fib_Face_Extended*)&(entry->faces);
    
    while (face->next) {
        face = (CefT_Fib_Face_Extended*)face->next;
        
        /* アクティブなFaceのみを対象とする */
        if (face->status == CefC_Face_Status_Active) {
            faceids[i] = face->faceid;
            i++;
            
            /* 最終アクティブ時刻を更新 */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            face->last_active_time = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
        }
    }
    
#ifdef CefC_Debug
    if (i > 0) {
        cef_dbg_write(CefC_Dbg_Finest, 
            "[FIB] Found %d active faces for forwarding\n", i);
    }
#endif
    
    return (i);
}

/*--------------------------------------------------------------------------------------
	Face単位でのステータス制御
----------------------------------------------------------------------------------------*/
int
cef_fib_face_status_set (
    CefT_Fib_Entry* entry,              /* FIBエントリ */
    uint16_t faceid,                    /* 対象Face-ID */
    uint8_t status                      /* 設定するステータス */
) {
    CefT_Fib_Face_Extended* face = (CefT_Fib_Face_Extended*)&(entry->faces);
    
    while (face->next) {
        face = (CefT_Fib_Face_Extended*)face->next;
        
        if (face->faceid == faceid) {
            face->status = status;
            
            /* ステータス変更時刻を記録 */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            face->last_active_time = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
            
#ifdef CefC_Debug
            cef_dbg_write(CefC_Dbg_Fine, 
                "[FIB] Face %d status set to %s\n", faceid,
                (status == CefC_Face_Status_Active) ? "ACTIVE" : "INACTIVE");
#endif
            return (0);
        }
    }
    
    return (-1);  // Face-IDが見つからない
}

/*--------------------------------------------------------------------------------------
	すべてのインアクティブエントリをクリーンアップ
----------------------------------------------------------------------------------------*/
void
cef_fib_inactive_entries_cleanup (
    CefT_Hash_Handle fib                /* FIB */
) {
    CefT_Fib_Entry_Extended* entry;
    CefT_Fib_Entry_Extended* work;
    uint32_t index = 0;
    int removed_count = 0;
    
    do {
        entry = (CefT_Fib_Entry_Extended*) cef_hash_tbl_item_check_from_index(fib, &index);
        
        if (entry && entry->status == CefC_Fib_Status_Inactive) {
            /* インアクティブなエントリを削除 */
            work = (CefT_Fib_Entry_Extended*) cef_hash_tbl_item_remove_from_index(fib, index);
            
            if (work) {
                free(work->key);
                free(work);
                removed_count++;
            }
        } else {
            index++;
        }
    } while (entry);
    
#ifdef CefC_Debug
    if (removed_count > 0) {
        cef_dbg_write(CefC_Dbg_Fine, 
            "[FIB] Cleaned up %d inactive entries\n", removed_count);
    }
#endif
}

/*--------------------------------------------------------------------------------------
	統計情報：アクティブ・インアクティブエントリ数を取得
----------------------------------------------------------------------------------------*/
void
cef_fib_status_statistics_get (
    CefT_Hash_Handle fib,               /* FIB */
    int* active_count,                  /* アクティブエントリ数 */
    int* inactive_count,                /* インアクティブエントリ数 */
    int* suspended_count                /* 一時停止エントリ数 */
) {
    CefT_Fib_Entry_Extended* entry;
    uint32_t index = 0;
    
    *active_count = 0;
    *inactive_count = 0;
    *suspended_count = 0;
    
    do {
        entry = (CefT_Fib_Entry_Extended*) cef_hash_tbl_item_check_from_index(fib, &index);
        
        if (entry) {
            switch (entry->status) {
                case CefC_Fib_Status_Active:
                    (*active_count)++;
                    break;
                case CefC_Fib_Status_Inactive:
                    (*inactive_count)++;
                    break;
                case CefC_Fib_Status_Suspended:
                    (*suspended_count)++;
                    break;
                default:
                    /* 未定義ステータスは非アクティブとして扱う */
                    (*inactive_count)++;
                    break;
            }
        }
        index++;
    } while (entry);
    
#ifdef CefC_Debug
    cef_dbg_write(CefC_Dbg_Fine, 
        "[FIB] Statistics: Active=%d, Inactive=%d, Suspended=%d\n", 
        *active_count, *inactive_count, *suspended_count);
#endif
}

/****************************************************************************************
 自動管理機能（オプション）
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
	使用頻度に基づく自動ステータス制御
----------------------------------------------------------------------------------------*/
void
cef_fib_auto_status_management (
    CefT_Hash_Handle fib,               /* FIB */
    uint64_t inactive_threshold_usec    /* 非アクティブと判定する時間（マイクロ秒） */
) {
    CefT_Fib_Entry_Extended* entry;
    uint32_t index = 0;
    struct timespec ts;
    uint64_t current_time;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    current_time = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    
    do {
        entry = (CefT_Fib_Entry_Extended*) cef_hash_tbl_item_check_from_index(fib, &index);
        
        if (entry && entry->status == CefC_Fib_Status_Active) {
            /* 最終使用時刻から閾値を超えていればインアクティブに変更 */
            if ((current_time - entry->last_used) > inactive_threshold_usec) {
                entry->status = CefC_Fib_Status_Inactive;
#ifdef CefC_Debug
                cef_dbg_write(CefC_Dbg_Fine, 
                    "[FIB] Auto-deactivated entry due to inactivity\n");
#endif
            }
        }
        index++;
    } while (entry);
} 