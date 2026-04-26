/* libhangul
 * Copyright (C) 2005-2009 Choe Hwanjin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#define strtok_r strtok_s
#endif

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hangul.h"
#include "hangulinternals.h"

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/**
 * @defgroup hanjadictionary 한자 사전 검색 기능
 *
 * @section hanjadictionaryusage 한자 사전 루틴의 사용 방법
 * libhangul에서는 한자 사전 파일과 그 사전 파일을 검색할 수 있는 몇가지
 * 함수의 셋을 제공한다. 여기에서 사용되는 모든 스트링은 UTF-8 인코딩을 
 * 사용한다. libhangul에서 사용하는 한자 사전 파일의 포맷은
 * @ref HanjaTable 섹션을 참조한다.
 * 
 * 그 개략적인 사용 방법은 다음과 같다.
 *
 * @code
    // 지정된 위치의 한자 사전 파일을 로딩한다.
    // 아래 코드에서는 libhangul의 한자 사전 파일을 로딩하기 위해서
    // NULL을 argument로 준다.
    HanjaTable* table = hanja_table_load(NULL);

    // "삼국사기"에 해당하는 한자를 찾는다.
    HanjaList* list = hanja_table_match_exact(table, "삼국사기");
    if (list != NULL) {
	int i;
	int n = hanja_list_get_size(list);
	for (i = 0; i < n; ++i) {
	    const char* hanja = hanja_list_get_nth_value(list);
	    printf("한자: %s\n", hanja);
	}
	hanja_list_delete(list);
    }
    
    hanja_table_delete(table);

 * @endcode
 */

/**
 * @file hanja.c
 */

/**
 * @ingroup hanjadictionary
 * @typedef Hanja
 * @brief 한자 사전 검색 결과의 최소 단위
 *
 * Hanja 오브젝트는 한자 사전 파일의 각 엔트리에 해당한다.
 * 각 엔트리는 키(key), 밸류(value) 페어로 볼 수 있는데, libhangul에서는
 * 약간 확장을 하여 설명(comment)도 포함하고 있다.
 * 한자 사전 포맷은 @ref HanjaTable 부분을 참조한다.
 *
 * 한자 사전을 검색하면 결과는 Hanja 오브젝트의 리스트 형태로 전달된다.
 * @ref HanjaList 에서 각 엔트리의 내용을 하나씩 확인할 수 있다.
 * @ref Hanja 의 멤버는 직접 참조할 수 없고, hanja_get_key(), hanja_get_value(),
 * hanja_get_comment() 함수로 찾아볼 수 있다.
 * char 스트링으로 전달되는 내용은 모두 UTF-8 인코딩으로 되어 있다.
 */

/**
 * @ingroup hanjadictionary
 * @typedef HanjaList
 * @brief 한자 사전의 검색 결과를 전달하는데 사용하는 오브젝트
 *
 * 한자 사전의 검색 함수를 사용하면 이 타입으로 결과를 리턴한다. 
 * 이 오브젝트에서 hanja_list_get_nth()함수를 이용하여 검색 결과를
 * 이터레이션할 수 있다.  내부 구현 내용은 외부로 노출되어 있지 않다.
 * @ref HanjaList 가 가지고 있는 아이템들은 accessor 함수들을 이용해서 참조한다.
 *
 * 참조: hanja_list_get_nth(), hanja_list_get_nth_key(),
 * hanja_list_get_nth_value(), hanja_list_get_nth_comment()
 */

/**
 * @ingroup hanjadictionary
 * @typedef HanjaTable
 * @brief 한자 사전을 관리하는데 사용하는 오브젝트
 *
 * libhangul에서 한자 사전을 관리하는데 사용하는 오브젝트로
 * 내부 구현 내용은 외부로 노출되어 있지 않다.
 *
 * libhangul에서 사용하는 한자 사전 파일의 포맷은 다음과 같은 형식이다.
 *
 * @code
 * # comment
 * key1:value1:comment1
 * key2:value2:comment2
 * key3:value3:comment3
 * ...
 * @endcode
 *
 * 각 필드는 @b @c : 으로 구분하고, 첫번째 필드는 각 한자를 찾을 키값이고 
 * 두번째 필드는 그 키값에 해당하는 한자 스트링, 세번째 필드는 이 키와
 * 값에 대한 설명이다. @# 으로 시작하는 라인은 주석으로 무시된다.
 *
 * 실제 예를 들면 다음과 같은 식이다.
 *
 * @code
 * 삼국사기:三國史記:삼국사기
 * 한자:漢字:한자
 * @endcode
 * 
 * 그 내용은 키값에 대해서 sorting 되어야 있어야 한다.
 * 파일의 인코딩은 UTF-8이어야 한다.
 */

typedef struct _HanjaIndex     HanjaIndex;

typedef struct _HanjaPair      HanjaPair;
typedef struct _HanjaPairArray HanjaPairArray;

struct _Hanja {
    uint32_t key_offset;
    uint32_t value_offset;
    uint32_t comment_offset;
};

struct _HanjaList {
    char*         key;
    size_t        len;
    size_t        alloc;
    const Hanja** items; 
};

struct _HanjaIndex {
    unsigned       offset;
    unsigned       count;
    const char*    key;
};

struct _HanjaTable {
    Hanja**        entries;
    unsigned       nentries;
    unsigned       entries_alloc;
    const Hanja**  key_entries;
    HanjaIndex*    keytable;
    unsigned       nkeys;
    const Hanja**  value_entries;
    HanjaIndex*    valuetable;
    unsigned       nvalues;
    FILE*          file;
};

typedef struct _HanjaBinaryHeader {
    char           magic[8];
    uint32_t       version;
    uint32_t       flags;
    uint64_t       source_hash;
    uint32_t       entry_count;
} HanjaBinaryHeader;

static const char hanja_binary_magic[8] = {'H', 'J', 'B', 'I', 'N', '1', '\0', '\0'};
static const uint32_t hanja_binary_version = 1;

struct _HanjaPair {
    ucschar first;
    ucschar second;
};

struct _HanjaPairArray {
    ucschar          key;
    const HanjaPair* pairs;
};

#include "hanjacompatible.h"

static const char utf8_skip_table[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

static inline int utf8_char_len(const char *p)
{
    return utf8_skip_table[*(const unsigned char*)p];
}

static inline const char* utf8_next(const char *str)
{
    int n = utf8_char_len(str);

    while (n > 0) {
	str++;
	if (*str == '\0')
	    return str;
	n--;
    }

    return str;
}

static inline char* utf8_prev(const char *str, const char *p)
{
    for (--p; p >= str; --p) {
	if ((*p & 0xc0) != 0x80)
	    break;
    }
    return (char*)p;
}

/* hanja searching functions */
static Hanja *
hanja_new(const char *key, const char *value, const char *comment)
{
    Hanja* hanja;
    size_t size;
    size_t keylen;
    size_t valuelen;
    size_t commentlen;
    char*  p;

    keylen = strlen(key) + 1;
    valuelen = strlen(value) + 1;
    if (comment != NULL)
	commentlen = strlen(comment) + 1;
    else
	commentlen = 1;

    size = sizeof(*hanja) + keylen + valuelen + commentlen;
    hanja = malloc(size);
    if (hanja == NULL)
	return NULL;

    p = (char*)hanja + sizeof(*hanja);
    strcpy(p, key);
    p += keylen;
    strcpy(p, value);
    p += valuelen;
    if (comment != NULL)
	strcpy(p, comment);
    else
	*p = '\0';
    p += valuelen;

    hanja->key_offset     = sizeof(*hanja);
    hanja->value_offset   = sizeof(*hanja) + keylen;
    hanja->comment_offset = sizeof(*hanja) + keylen + valuelen;

    return hanja;
}

static void
hanja_delete(Hanja* hanja)
{
    free(hanja);
}

const char*
hanja_get_key(const Hanja* hanja);

const char*
hanja_get_value(const Hanja* hanja);

const char*
hanja_get_comment(const Hanja* hanja);

static int
compare_hanja_key(const void* a, const void* b)
{
    const Hanja* left = *(const Hanja* const*)a;
    const Hanja* right = *(const Hanja* const*)b;
    return strcmp(hanja_get_key(left), hanja_get_key(right));
}

static int
compare_hanja_value(const void* a, const void* b)
{
    const Hanja* left = *(const Hanja* const*)a;
    const Hanja* right = *(const Hanja* const*)b;
    return strcmp(hanja_get_value(left), hanja_get_value(right));
}

static HanjaTable*
hanja_table_new(void)
{
    return calloc(1, sizeof(HanjaTable));
}

static int
hanja_table_append_entry(HanjaTable* table, const char* key, const char* value,
                         const char* comment)
{
    Hanja* hanja;
    Hanja** entries;
    unsigned alloc;

    if (table == NULL || key == NULL || value == NULL)
	return FALSE;

    if (table->nentries == table->entries_alloc) {
	alloc = table->entries_alloc == 0 ? 256 : table->entries_alloc * 2;
	entries = realloc(table->entries, alloc * sizeof(table->entries[0]));
	if (entries == NULL)
	    return FALSE;
	table->entries = entries;
	table->entries_alloc = alloc;
    }

    hanja = hanja_new(key, value, comment);
    if (hanja == NULL)
	return FALSE;

    table->entries[table->nentries++] = hanja;
    return TRUE;
}

static int
hanja_table_build_index_for_entries(const Hanja** sorted_entries,
				    unsigned nentries,
				    HanjaIndex** index_out,
				    unsigned* count_out,
				    int by_value)
{
    HanjaIndex* index;
    unsigned count = 0;
    unsigned i;
    unsigned item = 0;
    const char* previous = NULL;

    if (index_out == NULL || count_out == NULL)
	return FALSE;

    *index_out = NULL;
    *count_out = 0;

    if (nentries == 0)
	return TRUE;

    for (i = 0; i < nentries; ++i) {
	const char* key = by_value ? hanja_get_value(sorted_entries[i])
				   : hanja_get_key(sorted_entries[i]);
	if (previous == NULL || strcmp(previous, key) != 0) {
	    ++count;
	    previous = key;
	}
    }

    index = calloc(count, sizeof(index[0]));
    if (index == NULL)
	return FALSE;

    previous = NULL;
    for (i = 0; i < nentries; ++i) {
	const char* key = by_value ? hanja_get_value(sorted_entries[i])
				   : hanja_get_key(sorted_entries[i]);
	if (previous == NULL || strcmp(previous, key) != 0) {
	    if (item > 0)
		index[item - 1].count = i - index[item - 1].offset;
	    index[item].offset = i;
	    index[item].key = key;
	    ++item;
	    previous = key;
	}
    }
    index[item - 1].count = nentries - index[item - 1].offset;

    *index_out = index;
    *count_out = count;
    return TRUE;
}

static int
hanja_table_build_indexes(HanjaTable* table)
{
    if (table == NULL)
	return FALSE;

    free(table->key_entries);
    free(table->keytable);
    free(table->value_entries);
    free(table->valuetable);
    table->key_entries = NULL;
    table->keytable = NULL;
    table->value_entries = NULL;
    table->valuetable = NULL;
    table->nkeys = 0;
    table->nvalues = 0;

    if (table->nentries == 0)
	return TRUE;

    table->key_entries = malloc(table->nentries * sizeof(table->key_entries[0]));
    table->value_entries = malloc(table->nentries * sizeof(table->value_entries[0]));
    if (table->key_entries == NULL || table->value_entries == NULL)
	return FALSE;

    memcpy(table->key_entries, table->entries,
	   table->nentries * sizeof(table->key_entries[0]));
    memcpy(table->value_entries, table->entries,
	   table->nentries * sizeof(table->value_entries[0]));
    qsort(table->key_entries, table->nentries, sizeof(table->key_entries[0]),
	  compare_hanja_key);
    qsort(table->value_entries, table->nentries,
	  sizeof(table->value_entries[0]), compare_hanja_value);

    if (!hanja_table_build_index_for_entries(table->key_entries, table->nentries,
					     &table->keytable, &table->nkeys,
					     FALSE))
	return FALSE;

    if (!hanja_table_build_index_for_entries(table->value_entries, table->nentries,
					     &table->valuetable, &table->nvalues,
					     TRUE))
	return FALSE;

    return TRUE;
}

static HanjaIndex*
hanja_table_find_index(HanjaIndex* index, unsigned count, const char* key)
{
    int low;
    int high;

    if (index == NULL || key == NULL || count == 0)
	return NULL;

    low = 0;
    high = (int)count - 1;
    while (low <= high) {
	int mid = low + (high - low) / 2;
	int res = strcmp(index[mid].key, key);
	if (res < 0)
	    low = mid + 1;
	else if (res > 0)
	    high = mid - 1;
	else
	    return &index[mid];
    }

    return NULL;
}

static uint64_t
hanja_file_hash(const char* filename)
{
    FILE* file;
    uint64_t hash = 1469598103934665603ULL;
    unsigned char buffer[4096];
    size_t read_size;

    if (filename == NULL)
	return 0;

    file = fopen(filename, "rb");
    if (file == NULL)
	return 0;

    while ((read_size = fread(buffer, 1, sizeof(buffer), file)) > 0) {
	size_t i;
	for (i = 0; i < read_size; ++i) {
	    hash ^= buffer[i];
	    hash *= 1099511628211ULL;
	}
    }

    fclose(file);
    return hash;
}

/**
 * @ingroup hanjadictionary
 * @brief @ref Hanja 의 키를 찾아본다.
 * @return @a hanja 오브젝트의 키, UTF-8
 *
 * 일반적으로 @ref Hanja 아이템의 키는 한글이다.
 * 리턴되는 스트링은 @a hanja 오브젝트 내부적으로 관리하는 데이터로
 * 수정하거나 free 되어서는 안된다.
 */
const char*
hanja_get_key(const Hanja* hanja)
{
    if (hanja != NULL) {
	const char* p  = (const char*)hanja;
	return p + hanja->key_offset;
    }
    return NULL;
}

/**
 * @ingroup hanjadictionary
 * @brief @ref Hanja 의 값을 찾아본다.
 * @return @a hanja 오브젝트의 값, UTF-8
 *
 * 일반적으로 @ref Hanja 아이템의 값은 key에 대응되는 한자다.
 * 리턴되는 스트링은 @a hanja 오브젝트 내부적으로 관리하는 데이터로
 * 수정하거나 free되어서는 안된다.
 */
const char*
hanja_get_value(const Hanja* hanja)
{
    if (hanja != NULL) {
	const char* p  = (const char*)hanja;
	return p + hanja->value_offset;
    }
    return NULL;
}

/**
 * @ingroup hanjadictionary
 * @brief @ref Hanja 의 설명을 찾아본다.
 * @return @a hanja 오브젝트의 comment 필드, UTF-8
 *
 * 일반적으로 @ref Hanja 아이템의 설명은 한글과 그 한자에 대한 설명이다.
 * 파일에 따라서 내용이 없을 수 있다.
 * 리턴되는 스트링은 @a hanja 오브젝트 내부적으로 관리하는 데이터로
 * 수정하거나 free되어서는 안된다.
 */
const char*
hanja_get_comment(const Hanja* hanja)
{
    if (hanja != NULL) {
	const char* p  = (const char*)hanja;
	return p + hanja->comment_offset;
    }
    return NULL;
}

static HanjaList *
hanja_list_new(const char *key)
{
    HanjaList *list;

    list = malloc(sizeof(*list));
    if (list == NULL)
	return NULL;

    list->key = strdup(key);
    if (list->key == NULL) {
	free(list);
	return NULL;
    }

    list->len = 0;
    list->alloc = 1;
    list->items = malloc(list->alloc * sizeof(list->items[0]));
    if (list->items == NULL) {
	free(list->key);
	free(list);
	return NULL;
    }

    return list;
}

static void
hanja_list_reserve(HanjaList* list, size_t n)
{
    size_t size = list->alloc;

    if (n > SIZE_MAX / sizeof(list->items[0]) - list->len)
	return;

    while (size < list->len + n)
	size *= 2;

    if (size > SIZE_MAX / sizeof(list->items[0]))
	return;

    if (list->alloc < list->len + n) {
	const Hanja** data;

	data = realloc(list->items, size * sizeof(list->items[0]));
	if (data != NULL) {
	    list->alloc = size;
	    list->items = data;
	}
    }
}

static void
hanja_list_append_n(HanjaList* list, const Hanja* hanja, int n)
{
    hanja_list_reserve(list, n);

    if (list->alloc >= list->len + n) {
	unsigned int i;
	for (i = 0; i < n ; i++)
	    list->items[list->len + i] = hanja + i;
	list->len += n;
    }
}

static void
hanja_table_match(const HanjaTable* table,
		  const char* key, HanjaList** list)
{
    HanjaIndex* index;
    unsigned i;

    if (table == NULL || key == NULL || list == NULL)
	return;

    index = hanja_table_find_index(table->keytable, table->nkeys, key);
    if (index == NULL)
	return;

    if (*list == NULL)
	*list = hanja_list_new(key);
    if (*list == NULL)
	return;

    for (i = 0; i < index->count; ++i) {
	const Hanja* source = table->key_entries[index->offset + i];
	Hanja* hanja = hanja_new(hanja_get_key(source), hanja_get_value(source),
				 hanja_get_comment(source));
	if (hanja != NULL)
	    hanja_list_append_n(*list, hanja, 1);
    }
}

/**
 * @ingroup hanjadictionary
 * @brief 한자 사전 파일을 로딩하는 함수
 * @param filename 로딩할 사전 파일의 위치, 또는 NULL
 * @return 한자 사전 object 또는 NULL
 *
 * 이 함수는 한자 사전 파일을 로딩하는 함수로 @a filename으로 지정된 
 * 파일을 로딩한다. 한자 사전 파일은 libhangul에서 사용하는 포맷이어야 한다.
 * 한자 사전 파일의 포맷에 대한 정보는 HanjaTable을 참조한다.
 * 
 * @a filename은 locale에 따른 인코딩으로 되어 있어야 한다. UTF-8이 아닐 수
 * 있으므로 주의한다.
 * 
 * @a filename 에 NULL을 주면 libhangul에서 디폴트로 배포하는 사전을 로딩한다.
 * 파일이 없거나, 포맷이 맞지 않으면 로딩에 실패하고 NULL을 리턴한다.
 * 한자 사전이 더이상 필요없으면 hanja_table_delete() 함수로 삭제해야 한다.
 */
HanjaTable*
hanja_table_load(const char* filename)
{
    char buf[512];
    char* save_ptr = NULL;
    char* key;
    FILE* file;
    HanjaTable* table;

    if (filename == NULL)
#ifdef LIBHANGUL_DEFAULT_HANJA_DIC
	filename = LIBHANGUL_DEFAULT_HANJA_DIC;
#else
	return NULL;
#endif /* LIBHANGUL_DEFAULT_HANJA_DIC */

    file = fopen(filename, "r");
    if (file == NULL) {
	return NULL;
    }

    table = hanja_table_new();
    if (table == NULL) {
	fclose(file);
	return NULL;
    }

    while (fgets(buf, sizeof(buf), file) != NULL) {
	char* value;
	char* comment;

	/* skip comments and empty lines */
	if (buf[0] == '#' || buf[0] == '\r' || buf[0] == '\n' || buf[0] == '\0')
	    continue;

	save_ptr = NULL;
	key = strtok_r(buf, ":", &save_ptr);

	if (key == NULL || strlen(key) == 0)
	    continue;

	value = strtok_r(NULL, ":", &save_ptr);
	comment = strtok_r(NULL, "\r\n", &save_ptr);
	if (value == NULL || value[0] == '\0')
	    continue;

	if (!hanja_table_append_entry(table, key, value, comment)) {
	    hanja_table_delete(table);
	    fclose(file);
	    return NULL;
	}
    }

    fclose(file);
    if (!hanja_table_build_indexes(table)) {
	hanja_table_delete(table);
	return NULL;
    }

    return table;
}

HanjaTable*
hanja_table_load_binary(const char* binary_filename, const char* source_filename)
{
    FILE* file;
    HanjaBinaryHeader header;
    HanjaTable* table;
    uint32_t i;
    uint64_t source_hash = 0;

    if (binary_filename == NULL)
	return NULL;

    file = fopen(binary_filename, "rb");
    if (file == NULL)
	return NULL;

    if (fread(&header, sizeof(header), 1, file) != 1) {
	fclose(file);
	return NULL;
    }

    if (memcmp(header.magic, hanja_binary_magic, sizeof(header.magic)) != 0 ||
	header.version != hanja_binary_version) {
	fclose(file);
	return NULL;
    }

    if (source_filename != NULL) {
	source_hash = hanja_file_hash(source_filename);
	if (source_hash == 0 || header.source_hash != source_hash) {
	    fclose(file);
	    return NULL;
	}
    }

    table = hanja_table_new();
    if (table == NULL) {
	fclose(file);
	return NULL;
    }

    for (i = 0; i < header.entry_count; ++i) {
	uint32_t key_len;
	uint32_t value_len;
	uint32_t comment_len;
	char* key = NULL;
	char* value = NULL;
	char* comment = NULL;

	if (fread(&key_len, sizeof(key_len), 1, file) != 1 ||
	    fread(&value_len, sizeof(value_len), 1, file) != 1 ||
	    fread(&comment_len, sizeof(comment_len), 1, file) != 1) {
	    hanja_table_delete(table);
	    fclose(file);
	    return NULL;
	}

	key = malloc((size_t)key_len + 1);
	value = malloc((size_t)value_len + 1);
	comment = malloc((size_t)comment_len + 1);
	if (key == NULL || value == NULL || comment == NULL) {
	    free(key);
	    free(value);
	    free(comment);
	    hanja_table_delete(table);
	    fclose(file);
	    return NULL;
	}

	if (fread(key, 1, key_len, file) != key_len ||
	    fread(value, 1, value_len, file) != value_len ||
	    fread(comment, 1, comment_len, file) != comment_len) {
	    free(key);
	    free(value);
	    free(comment);
	    hanja_table_delete(table);
	    fclose(file);
	    return NULL;
	}
	key[key_len] = '\0';
	value[value_len] = '\0';
	comment[comment_len] = '\0';

	if (!hanja_table_append_entry(table, key, value, comment)) {
	    free(key);
	    free(value);
	    free(comment);
	    hanja_table_delete(table);
	    fclose(file);
	    return NULL;
	}
	free(key);
	free(value);
	free(comment);
    }

    fclose(file);
    if (!hanja_table_build_indexes(table)) {
	hanja_table_delete(table);
	return NULL;
    }

    return table;
}

int
hanja_table_save_binary(const HanjaTable* table, const char* binary_filename,
			const char* source_filename)
{
    FILE* file;
    HanjaBinaryHeader header;
    unsigned i;

    if (table == NULL || binary_filename == NULL)
	return FALSE;

    file = fopen(binary_filename, "wb");
    if (file == NULL)
	return FALSE;

    memset(&header, 0, sizeof(header));
    memcpy(header.magic, hanja_binary_magic, sizeof(header.magic));
    header.version = hanja_binary_version;
    header.flags = 0;
    header.source_hash = hanja_file_hash(source_filename);
    header.entry_count = table->nentries;

    if (fwrite(&header, sizeof(header), 1, file) != 1) {
	fclose(file);
	return FALSE;
    }

    for (i = 0; i < table->nentries; ++i) {
	const char* key = hanja_get_key(table->entries[i]);
	const char* value = hanja_get_value(table->entries[i]);
	const char* comment = hanja_get_comment(table->entries[i]);
	uint32_t key_len = (uint32_t)strlen(key);
	uint32_t value_len = (uint32_t)strlen(value);
	uint32_t comment_len = (uint32_t)strlen(comment);

	if (fwrite(&key_len, sizeof(key_len), 1, file) != 1 ||
	    fwrite(&value_len, sizeof(value_len), 1, file) != 1 ||
	    fwrite(&comment_len, sizeof(comment_len), 1, file) != 1 ||
	    fwrite(key, 1, key_len, file) != key_len ||
	    fwrite(value, 1, value_len, file) != value_len ||
	    fwrite(comment, 1, comment_len, file) != comment_len) {
	    fclose(file);
	    return FALSE;
	}
    }

    fclose(file);
    return TRUE;
}

HanjaTable*
hanja_table_load_with_binary(const char* filename, const char* binary_filename)
{
    HanjaTable* table = NULL;

    if (binary_filename != NULL) {
	table = hanja_table_load_binary(binary_filename, filename);
	if (table != NULL)
	    return table;
	if (filename == NULL) {
	    table = hanja_table_load_binary(binary_filename, NULL);
	    if (table != NULL)
		return table;
	}
    }

    table = hanja_table_load(filename);
    if (table != NULL && binary_filename != NULL)
	hanja_table_save_binary(table, binary_filename, filename);

    return table;
}

/**
 * @ingroup hanjadictionary
 * @brief 한자 사전 object를 free하는 함수
 * @param table free할 한자 사전 object
 */
void
hanja_table_delete(HanjaTable *table)
{
    if (table != NULL) {
	unsigned i;
	if (table->entries != NULL) {
	    for (i = 0; i < table->nentries; ++i)
		hanja_delete(table->entries[i]);
	}
	free(table->entries);
	free(table->key_entries);
	free(table->keytable);
	free(table->value_entries);
	free(table->valuetable);
	if (table->file != NULL)
	    fclose(table->file);
	free(table);
    }
}

/**
 * @ingroup hanjadictionary
 * @brief 한자 사전에서 매치되는 키를 가진 엔트리를 찾는 함수
 * @param table 한자 사전 object
 * @param key 찾을 키, UTF-8 인코딩
 * @return 찾은 결과를 HanjaList object로 리턴한다. 찾은 것이 없거나 에러가 
 *         있으면 NULL을 리턴한다.
 *
 * @a key 값과 같은 키를 가진 엔트리를 검색한다.
 * 리턴된 결과는 다 사용하고 나면 반드시 hanja_list_delete() 함수로 free해야
 * 한다.
 */
HanjaList*
hanja_table_match_exact(const HanjaTable* table, const char *key)
{
    HanjaList* ret = NULL;

    if (key == NULL || key[0] == '\0' || table == NULL)
	return NULL;

    hanja_table_match(table, key, &ret);

    return ret;
}

HanjaList*
hanja_table_match_exact_value(const HanjaTable* table, const char *value)
{
    HanjaList* ret = NULL;
    HanjaIndex* index;
    unsigned i;

    if (value == NULL || value[0] == '\0' || table == NULL)
	return NULL;

    index = hanja_table_find_index(table->valuetable, table->nvalues, value);
    if (index == NULL)
	return NULL;

    ret = hanja_list_new(value);
    if (ret == NULL)
	return NULL;

    for (i = 0; i < index->count; ++i) {
	const Hanja* source = table->value_entries[index->offset + i];
	Hanja* hanja = hanja_new(value, hanja_get_key(source),
				 hanja_get_comment(source));
	if (hanja != NULL)
	    hanja_list_append_n(ret, hanja, 1);
    }

    return ret;
}

/**
 * @ingroup hanjadictionary
 * @brief 한자 사전에서 앞부분이 매치되는 키를 가진 엔트리를 찾는 함수
 * @param table 한자 사전 object
 * @param key 찾을 키, UTF-8 인코딩
 * @return 찾은 결과를 HanjaList object로 리턴한다. 찾은 것이 없거나 에러가 
 *         있으면 NULL을 리턴한다.
 *
 * @a key 값과 같거나 앞부분이 같은 키를 가진 엔트리를 검색한다.
 * 그리고 key를 뒤에서부터 한자씩 줄여가면서 검색을 계속한다.
 * 예로 들면 "삼국사기"를 검색하면 "삼국사기", "삼국사", "삼국", "삼"을 
 * 각각 모두 검색한다.
 * 리턴된 결과는 다 사용하고 나면 반드시 hanja_list_delete() 함수로 free해야
 * 한다.
 */
HanjaList*
hanja_table_match_prefix(const HanjaTable* table, const char *key)
{
    char* p;
    char* newkey;
    HanjaList* ret = NULL;

    if (key == NULL || key[0] == '\0' || table == NULL)
	return NULL;

    newkey = strdup(key);
    if (newkey == NULL)
	return NULL;

    p = strchr(newkey, '\0');
    while (newkey[0] != '\0') {
	hanja_table_match(table, newkey, &ret);
	p = utf8_prev(newkey, p);
	p[0] = '\0';
    }
    free(newkey);

    return ret;
}

/**
 * @ingroup hanjadictionary
 * @brief 한자 사전에서 뒷부분이 매치되는 키를 가진 엔트리를 찾는 함수
 * @param table 한자 사전 object
 * @param key 찾을 키, UTF-8 인코딩
 * @return 찾은 결과를 HanjaList object로 리턴한다. 찾은 것이 없거나 에러가 
 *         있으면 NULL을 리턴한다.
 *
 * @a key 값과 같거나 뒷부분이 같은 키를 가진 엔트리를 검색한다.
 * 그리고 key를 앞에서부터 한자씩 줄여가면서 검색을 계속한다.
 * 예로 들면 "삼국사기"를 검색하면 "삼국사기", "국사기", "사기", "기"를 
 * 각각 모두 검색한다.
 * 리턴된 결과는 다 사용하고 나면 반드시 hanja_list_delete() 함수로 free해야
 * 한다.
 */
HanjaList*
hanja_table_match_suffix(const HanjaTable* table, const char *key)
{
    const char* p;
    HanjaList* ret = NULL;

    if (key == NULL || key[0] == '\0' || table == NULL)
	return NULL;

    p = key;
    while (p[0] != '\0') {
	hanja_table_match(table, p, &ret);
	p = utf8_next(p);
    }

    return ret;
}

/**
 * @ingroup hanjadictionary
 * @brief @ref HanjaList 가 가지고 있는 아이템의 갯수를 구하는 함수
 */
int
hanja_list_get_size(const HanjaList *list)
{
    if (list != NULL)
	return list->len;
    return 0;
}

/**
 * @ingroup hanjadictionary
 * @brief @ref HanjaList 가 생성될때 검색함수에서 사용한 키를 구하는 함수
 * @return @ref HanjaList 의 key 스트링
 *
 * 한자 사전 검색 함수로 @ref HanjaList 를 생성하면 @ref HanjaList 는
 * 그 검색할때 사용한 키를 기억하고 있다. 이 값을 확인할때 사용한다.
 * 주의할 점은, 각 Hanja 아이템들은 각각의 키를 가지고 있지만, 이것이
 * 반드시 @ref HanjaList 와 일치하지는 않는다는 것이다.
 * 검색할 당시에 사용한 함수가 prefix나 suffix계열이면 더 짧은 키로도 
 * 검색하기 때문에 @ref HanjaList 의 키와 검색 결과의 키와 다른 것들도 
 * 가지고 있게 된다.
 *
 * 리턴된 스트링 포인터는 @ref HanjaList 에서 관리하는 스트링으로
 * 수정하거나 free해서는 안된다.
 */
const char*
hanja_list_get_key(const HanjaList *list)
{
    if (list != NULL)
	return list->key;
    return NULL;
}

/**
 * @ingroup hanjadictionary
 * @brief @ref HanjaList 의 n번째 @ref Hanja 아이템의 포인터를 구하는 함수
 * @param list @ref HanjaList 를 가리키는 포인터
 * @param n 참조할 아이템의 인덱스
 * @return @ref Hanja 를 가리키는 포인터
 * 
 * 이 함수는 @a list가 가리키는 @ref HanjaList 의 n번째 @ref Hanja 오브젝트를
 * 가리키는 포인터를 리턴한다.
 * @ref HanjaList 의 각 아이템은 정수형 인덱스로 각각 참조할 수 있다.
 * @ref HanjaList 가 가진 엔트리 갯수를 넘어서는 인덱스를 주면 NULL을 리턴한다.
 * 리턴된 @ref Hanja 오브젝트는 @ref HanjaList 가 관리하는 오브젝트로 free하거나
 * 수정해서는 안된다.
 *
 * 다음의 예제는 list로 주어진 @ref HanjaList 의 모든 값을 프린트 하는 
 * 코드다.
 * 
 * @code
 * int i;
 * int n = hanja_list_get_size(list);
 * for (i = 0; i < n; i++) {
 *	Hanja* hanja = hanja_list_get_nth(i);
 *	const char* value = hanja_get_value(hanja);
 *	printf("Hanja: %s\n", value);
 *	// 또는 hanja에서 다른 정보를 참조하거나
 *	// 다른 작업을 할 수도 있다.
 * }
 * @endcode
 */
const Hanja*
hanja_list_get_nth(const HanjaList *list, unsigned int n)
{
    if (list != NULL) {
	if (n < list->len)
	    return list->items[n];
    }
    return NULL;
}

/**
 * @ingroup hanjadictionary
 * @brief @ref HanjaList 의 n번째 아이템의 키를 구하는 함수
 * @return n번째 아이템의 키, UTF-8
 *
 * HanjaList_get_nth()의 convenient 함수
 */
const char*
hanja_list_get_nth_key(const HanjaList *list, unsigned int n)
{
    const Hanja* hanja = hanja_list_get_nth(list, n);
    return hanja_get_key(hanja);
}

/**
 * @ingroup hanjadictionary
 * @brief @ref HanjaList 의 n번째 아이템의 값를 구하는 함수
 * @return n번째 아이템의 값(value), UTF-8
 *
 * HanjaList_get_nth()의 convenient 함수
 */
const char*
hanja_list_get_nth_value(const HanjaList *list, unsigned int n)
{
    const Hanja* hanja = hanja_list_get_nth(list, n);
    return hanja_get_value(hanja);
}

/**
 * @ingroup hanjadictionary
 * @brief @ref HanjaList 의 n번째 아이템의 설명을 구하는 함수
 * @return n번째 아이템의 설명(comment), UTF-8
 *
 * HanjaList_get_nth()의 convenient 함수
 */
const char*
hanja_list_get_nth_comment(const HanjaList *list, unsigned int n)
{
    const Hanja* hanja = hanja_list_get_nth(list, n);
    return hanja_get_comment(hanja);
}

/**
 * @ingroup hanjadictionary
 * @brief 한자 사전 검색 함수가 리턴한 결과를 free하는 함수
 * @param list free할 @ref HanjaList
 *
 * libhangul의 모든 한자 사전 검색 루틴이 리턴한 결과는 반드시
 * 이 함수로 free해야 한다.
 */
void
hanja_list_delete(HanjaList *list)
{
    if (list) {
	size_t i;
	for (i = 0; i < list->len; i++) {
	    hanja_delete((Hanja*)list->items[i]);
	}
	free(list->items);
	free(list->key);
	free(list);
    }
}

static int
compare_pair(const void* a, const void* b)
{
    const ucschar*   c = a;
    const HanjaPair* y = b;

    return *c - y->first;
}

size_t
hanja_compatibility_form(ucschar* hanja, const ucschar* hangul, size_t n)
{
    size_t i;
    size_t nconverted;

    if (hangul == NULL || hanja == NULL)
	return 0;

    nconverted = 0;
    for (i = 0; i < n && hangul[i] != 0 && hanja[i] != 0; i++) {
	HanjaPairArray* p;

	p = bsearch(&hanja[i],
		    hanja_unified_to_compat_table,
		    N_ELEMENTS(hanja_unified_to_compat_table),
		    sizeof(hanja_unified_to_compat_table[0]),
		    compare_pair);
	if (p != NULL) {
	    const HanjaPair* pair = p->pairs;
	    while (pair->first != 0) {
		if (pair->first == hangul[i]) {
		    hanja[i] = pair->second;
		    nconverted++;
		    break;
		}
		pair++;
	    }
	}
    }

    return nconverted;
}

size_t
hanja_unified_form(ucschar* str, size_t n)
{
    size_t i;
    size_t nconverted;

    if (str == NULL)
	return 0;

    nconverted = 0;
    for (i = 0; i < n && str[i] != 0; i++) {
	if (str[i] >= 0xF900 && str[i] <= 0xFA0B) {
	    str[i] = hanja_compat_to_unified_table[str[i] - 0xF900];
	    nconverted++;
	}
    }

    return nconverted;
}
