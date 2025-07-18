/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ParserAtom_h
#define frontend_ParserAtom_h

#include "mozilla/DebugOnly.h"        // mozilla::DebugOnly
#include "mozilla/HashFunctions.h"    // mozilla::HashString
#include "mozilla/MemoryReporting.h"  // mozilla::MallocSizeOf
#include "mozilla/Range.h"            // mozilla::Range
#include "mozilla/Span.h"             // mozilla::Span
#include "mozilla/Variant.h"          // mozilla::Variant

#include "ds/LifoAlloc.h"         // LifoAlloc
#include "frontend/TypedIndex.h"  // TypedIndex
#include "js/HashTable.h"         // HashMap
#include "js/UniquePtr.h"         // js::UniquePtr
#include "js/Vector.h"            // Vector
#include "vm/CommonPropertyNames.h"
#include "vm/StringType.h"     // CompareChars, StringEqualsAscii
#include "vm/WellKnownAtom.h"  // WellKnownAtomId, WellKnownAtomInfo

namespace js {

class StringBuffer;

namespace frontend {

struct CompilationAtomCache;
struct CompilationStencil;
class BorrowingCompilationStencil;
class ParserAtom;

template <typename CharT>
class SpecificParserAtomLookup;

class ParserAtomsTable;

// These types correspond into indices in the StaticStrings arrays.
enum class Length1StaticParserString : uint8_t;
enum class Length2StaticParserString : uint16_t;

class ParserAtom;
using ParserAtomIndex = TypedIndex<ParserAtom>;

// ParserAtomIndex, WellKnownAtomId, Length1StaticParserString,
// Length2StaticParserString, or null.
//
// 0x0000_0000  Null atom
//
// 0x1YYY_YYYY  28-bit ParserAtom
//
// 0x2000_YYYY  Well-known atom ID
// 0x2001_YYYY  Static length-1 atom
// 0x2002_YYYY  Static length-2 atom
class TaggedParserAtomIndex {
  uint32_t data_;

 public:
  static constexpr size_t IndexBit = 28;
  static constexpr size_t IndexMask = BitMask(IndexBit);

  static constexpr size_t TagShift = IndexBit;
  static constexpr size_t TagBit = 4;
  static constexpr size_t TagMask = BitMask(TagBit) << TagShift;

  enum class Kind : uint32_t {
    Null = 0,
    ParserAtomIndex,
    WellKnown,
  };

 private:
  static constexpr size_t SmallIndexBit = 16;
  static constexpr size_t SmallIndexMask = BitMask(SmallIndexBit);

  static constexpr size_t SubTagShift = SmallIndexBit;
  static constexpr size_t SubTagBit = 2;
  static constexpr size_t SubTagMask = BitMask(SubTagBit) << SubTagShift;

 public:
  static constexpr uint32_t NullTag = uint32_t(Kind::Null) << TagShift;
  static constexpr uint32_t ParserAtomIndexTag = uint32_t(Kind::ParserAtomIndex)
                                                 << TagShift;
  static constexpr uint32_t WellKnownTag = uint32_t(Kind::WellKnown)
                                           << TagShift;

 private:
  static constexpr uint32_t WellKnownSubTag = 0 << SubTagShift;
  static constexpr uint32_t Length1StaticSubTag = 1 << SubTagShift;
  static constexpr uint32_t Length2StaticSubTag = 2 << SubTagShift;

 public:
  static constexpr uint32_t IndexLimit = Bit(IndexBit);
  static constexpr uint32_t SmallIndexLimit = Bit(SmallIndexBit);

  static constexpr size_t Length1StaticLimit = 128U;
  static constexpr size_t Length2StaticLimit =
      StaticStrings::NUM_LENGTH2_ENTRIES;

 private:
  explicit TaggedParserAtomIndex(uint32_t data) : data_(data) {}

 public:
  constexpr TaggedParserAtomIndex() : data_(NullTag) {}

  explicit constexpr TaggedParserAtomIndex(ParserAtomIndex index)
      : data_(index.index | ParserAtomIndexTag) {
    MOZ_ASSERT(index.index < IndexLimit);
  }
  explicit constexpr TaggedParserAtomIndex(WellKnownAtomId index)
      : data_(uint32_t(index) | WellKnownTag | WellKnownSubTag) {
    MOZ_ASSERT(uint32_t(index) < SmallIndexLimit);

    // Length1Static/Length2Static string shouldn't use WellKnownAtomId.
#define CHECK_(_, NAME, _2) MOZ_ASSERT(index != WellKnownAtomId::NAME);
    FOR_EACH_NON_EMPTY_TINY_PROPERTYNAME(CHECK_)
#undef CHECK_
  }
  explicit constexpr TaggedParserAtomIndex(Length1StaticParserString index)
      : data_(uint32_t(index) | WellKnownTag | Length1StaticSubTag) {}
  explicit constexpr TaggedParserAtomIndex(Length2StaticParserString index)
      : data_(uint32_t(index) | WellKnownTag | Length2StaticSubTag) {}

  class WellKnown {
   public:
#define METHOD_(_, NAME, _2)                             \
  static constexpr TaggedParserAtomIndex NAME() {        \
    return TaggedParserAtomIndex(WellKnownAtomId::NAME); \
  }
    FOR_EACH_NONTINY_COMMON_PROPERTYNAME(METHOD_)
#undef METHOD_

#define METHOD_(NAME, _)                                 \
  static constexpr TaggedParserAtomIndex NAME() {        \
    return TaggedParserAtomIndex(WellKnownAtomId::NAME); \
  }
    JS_FOR_EACH_PROTOTYPE(METHOD_)
#undef METHOD_

#define METHOD_(NAME)                                    \
  static constexpr TaggedParserAtomIndex NAME() {        \
    return TaggedParserAtomIndex(WellKnownAtomId::NAME); \
  }
    JS_FOR_EACH_WELL_KNOWN_SYMBOL(METHOD_)
#undef METHOD_

#define METHOD_(_, NAME, STR)                                          \
  static constexpr TaggedParserAtomIndex NAME() {                      \
    return TaggedParserAtomIndex(Length1StaticParserString((STR)[0])); \
  }
    FOR_EACH_LENGTH1_PROPERTYNAME(METHOD_)
#undef METHOD_

#define METHOD_(_, NAME, STR)                                         \
  static constexpr TaggedParserAtomIndex NAME() {                     \
    return TaggedParserAtomIndex(Length2StaticParserString(           \
        (StaticStrings::getLength2IndexStatic((STR)[0], (STR)[1])))); \
  }
    FOR_EACH_LENGTH2_PROPERTYNAME(METHOD_)
#undef METHOD_

    static constexpr TaggedParserAtomIndex empty() {
      return TaggedParserAtomIndex(WellKnownAtomId::empty);
    }
  };

  // The value of rawData() for WellKnown TaggedParserAtomIndex.
  // For using in switch-case.
  class WellKnownRawData {
   public:
#define METHOD_(_, NAME, _2)                                                 \
  static constexpr uint32_t NAME() {                                         \
    return uint32_t(WellKnownAtomId::NAME) | WellKnownTag | WellKnownSubTag; \
  }
    FOR_EACH_NONTINY_COMMON_PROPERTYNAME(METHOD_)
#undef METHOD_

#define METHOD_(NAME, _)                                                     \
  static constexpr uint32_t NAME() {                                         \
    return uint32_t(WellKnownAtomId::NAME) | WellKnownTag | WellKnownSubTag; \
  }
    JS_FOR_EACH_PROTOTYPE(METHOD_)
#undef METHOD_

#define METHOD_(NAME)                                                        \
  static constexpr uint32_t NAME() {                                         \
    return uint32_t(WellKnownAtomId::NAME) | WellKnownTag | WellKnownSubTag; \
  }
    JS_FOR_EACH_WELL_KNOWN_SYMBOL(METHOD_)
#undef METHOD_

#define METHOD_(_, NAME, STR)                                       \
  static constexpr uint32_t NAME() {                                \
    return uint32_t((STR)[0]) | WellKnownTag | Length1StaticSubTag; \
  }
    FOR_EACH_LENGTH1_PROPERTYNAME(METHOD_)
#undef METHOD_

#define METHOD_(_, NAME, STR)                                              \
  static constexpr uint32_t NAME() {                                       \
    return uint32_t(                                                       \
               StaticStrings::getLength2IndexStatic((STR)[0], (STR)[1])) | \
           WellKnownTag | Length2StaticSubTag;                             \
  }
    FOR_EACH_LENGTH2_PROPERTYNAME(METHOD_)
#undef METHOD_

    static constexpr uint32_t empty() {
      return uint32_t(WellKnownAtomId::empty) | WellKnownTag | WellKnownSubTag;
    }
  };

  // NOTE: this is not well-known "null".
  static TaggedParserAtomIndex null() { return TaggedParserAtomIndex(); }

#ifdef DEBUG
  void validateRaw();
#endif

  static TaggedParserAtomIndex fromRaw(uint32_t data) {
    auto result = TaggedParserAtomIndex(data);
#ifdef DEBUG
    result.validateRaw();
#endif
    return result;
  }

  bool isParserAtomIndex() const {
    return (data_ & TagMask) == ParserAtomIndexTag;
  }
  bool isWellKnownAtomId() const {
    return (data_ & (TagMask | SubTagMask)) == (WellKnownTag | WellKnownSubTag);
  }
  bool isLength1StaticParserString() const {
    return (data_ & (TagMask | SubTagMask)) ==
           (WellKnownTag | Length1StaticSubTag);
  }
  bool isLength2StaticParserString() const {
    return (data_ & (TagMask | SubTagMask)) ==
           (WellKnownTag | Length2StaticSubTag);
  }
  bool isNull() const {
    bool result = !data_;
    MOZ_ASSERT_IF(result, (data_ & TagMask) == NullTag);
    return result;
  }

  ParserAtomIndex toParserAtomIndex() const {
    MOZ_ASSERT(isParserAtomIndex());
    return ParserAtomIndex(data_ & IndexMask);
  }
  WellKnownAtomId toWellKnownAtomId() const {
    MOZ_ASSERT(isWellKnownAtomId());
    return WellKnownAtomId(data_ & SmallIndexMask);
  }
  Length1StaticParserString toLength1StaticParserString() const {
    MOZ_ASSERT(isLength1StaticParserString());
    return Length1StaticParserString(data_ & SmallIndexMask);
  }
  Length2StaticParserString toLength2StaticParserString() const {
    MOZ_ASSERT(isLength2StaticParserString());
    return Length2StaticParserString(data_ & SmallIndexMask);
  }

  uint32_t* rawDataRef() { return &data_; }
  uint32_t rawData() const { return data_; }

  bool operator==(const TaggedParserAtomIndex& rhs) const {
    return data_ == rhs.data_;
  }
  bool operator!=(const TaggedParserAtomIndex& rhs) const {
    return data_ != rhs.data_;
  }

  explicit operator bool() const { return !isNull(); }
};

// Trivial variant of TaggedParserAtomIndex, to use in collection that requires
// trivial type.
// Provides minimal set of methods to use in collection.
class TrivialTaggedParserAtomIndex {
  uint32_t data_;

 public:
  static TrivialTaggedParserAtomIndex from(TaggedParserAtomIndex index) {
    TrivialTaggedParserAtomIndex result;
    result.data_ = index.rawData();
    return result;
  }

  operator TaggedParserAtomIndex() const {
    return TaggedParserAtomIndex::fromRaw(data_);
  }

  static TrivialTaggedParserAtomIndex null() {
    TrivialTaggedParserAtomIndex result;
    result.data_ = 0;
    return result;
  }

  bool isNull() const {
    static_assert(TaggedParserAtomIndex::NullTag == 0);
    return data_ == 0;
  }

  uint32_t rawData() const { return data_; }

  bool operator==(const TrivialTaggedParserAtomIndex& rhs) const {
    return data_ == rhs.data_;
  }
  bool operator!=(const TrivialTaggedParserAtomIndex& rhs) const {
    return data_ != rhs.data_;
  }

  explicit operator bool() const { return !isNull(); }
};

/**
 * A ParserAtom is an in-parser representation of an interned atomic
 * string.  It mostly mirrors the information carried by a JSAtom*.
 *
 * The atom contents are stored in one of two locations:
 *  1. Inline Latin1Char storage (immediately after the ParserAtom memory).
 *  2. Inline char16_t storage (immediately after the ParserAtom memory).
 */
class alignas(alignof(uint32_t)) ParserAtom {
  friend class ParserAtomsTable;
  friend class WellKnownParserAtoms;

  static const uint16_t MAX_LATIN1_CHAR = 0xff;

  // Bit flags inside flags_.
  static constexpr uint32_t HasTwoByteCharsFlag = 1 << 0;
  static constexpr uint32_t UsedByStencilFlag = 1 << 1;

  // Helper routine to read some sequence of two-byte chars, and write them
  // into a target buffer of a particular character width.
  //
  // The characters in the sequence must have been verified prior
  template <typename CharT, typename SeqCharT>
  static void drainChar16Seq(CharT* buf, InflatedChar16Sequence<SeqCharT> seq,
                             uint32_t length) {
    static_assert(
        std::is_same_v<CharT, char16_t> || std::is_same_v<CharT, Latin1Char>,
        "Invalid target buffer type.");
    CharT* cur = buf;
    while (seq.hasMore()) {
      char16_t ch = seq.next();
      if constexpr (std::is_same_v<CharT, Latin1Char>) {
        MOZ_ASSERT(ch <= MAX_LATIN1_CHAR);
      }
      MOZ_ASSERT(cur < (buf + length));
      *cur = ch;
      cur++;
    }
  }

 private:
  // The JSAtom-compatible hash of the string.
  HashNumber hash_ = 0;

  // The length of the buffer in chars_.
  uint32_t length_ = 0;

  uint32_t flags_ = 0;

  // End of fields.

  static const uint32_t MAX_LENGTH = JSString::MAX_LENGTH;

  ParserAtom(uint32_t length, HashNumber hash, bool hasTwoByteChars)
      : hash_(hash),
        length_(length),
        flags_(hasTwoByteChars ? HasTwoByteCharsFlag : 0) {}

 public:
  // The constexpr constructor is used by XDR
  constexpr ParserAtom() = default;

  // ParserAtoms may own their content buffers in variant_, and thus
  // cannot be copy-constructed - as a new chars would need to be allocated.
  ParserAtom(const ParserAtom&) = delete;
  ParserAtom(ParserAtom&& other) = delete;

  template <typename CharT, typename SeqCharT>
  static ParserAtom* allocate(JSContext* cx, LifoAlloc& alloc,
                              InflatedChar16Sequence<SeqCharT> seq,
                              uint32_t length, HashNumber hash);

  bool hasLatin1Chars() const { return !(flags_ & HasTwoByteCharsFlag); }
  bool hasTwoByteChars() const { return flags_ & HasTwoByteCharsFlag; }

  bool isAscii() const {
    if (hasTwoByteChars()) {
      return false;
    }
    for (Latin1Char ch : latin1Range()) {
      if (!mozilla::IsAscii(ch)) {
        return false;
      }
    }
    return true;
  }

  HashNumber hash() const { return hash_; }
  uint32_t length() const { return length_; }

  bool isUsedByStencil() const { return flags_ & UsedByStencilFlag; }

  template <typename CharT>
  bool equalsSeq(HashNumber hash, InflatedChar16Sequence<CharT> seq) const;

  // Convert NotInstantiated and usedByStencil entry to a js-atom.
  JSAtom* instantiate(JSContext* cx, ParserAtomIndex index,
                      CompilationAtomCache& atomCache) const;

 private:
  void markUsedByStencil() { flags_ |= UsedByStencilFlag; }

  constexpr void setHashAndLength(HashNumber hash, uint32_t length) {
    hash_ = hash;
    length_ = length;
  }

  template <typename CharT>
  const CharT* chars() const {
    MOZ_ASSERT(sizeof(CharT) == (hasTwoByteChars() ? 2 : 1));
    return reinterpret_cast<const CharT*>(this + 1);
  }

  template <typename CharT>
  CharT* chars() {
    MOZ_ASSERT(sizeof(CharT) == (hasTwoByteChars() ? 2 : 1));
    return reinterpret_cast<CharT*>(this + 1);
  }

  const Latin1Char* latin1Chars() const { return chars<Latin1Char>(); }
  const char16_t* twoByteChars() const { return chars<char16_t>(); }
  mozilla::Range<const Latin1Char> latin1Range() const {
    return mozilla::Range(latin1Chars(), length_);
  }
  mozilla::Range<const char16_t> twoByteRange() const {
    return mozilla::Range(twoByteChars(), length_);
  }

  // Returns index-th char.
  // Boundary check isn't performed.
  char16_t charAt(size_t index) const {
    MOZ_ASSERT(index < length());
    if (hasLatin1Chars()) {
      return latin1Chars()[index];
    }
    return twoByteChars()[index];
  }

 public:
#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump() const;
  void dumpCharsNoQuote(js::GenericPrinter& out) const;
#endif
};

/**
 * A lookup structure that allows for querying ParserAtoms in
 * a hashtable using a flexible input type that supports string
 * representations of various forms.
 */
class ParserAtomLookup {
 protected:
  HashNumber hash_;

  ParserAtomLookup(HashNumber hash) : hash_(hash) {}

 public:
  HashNumber hash() const { return hash_; }

  virtual bool equalsEntry(const ParserAtom* entry) const = 0;
  virtual bool equalsEntry(const WellKnownAtomInfo* info) const = 0;
};

struct ParserAtomLookupHasher {
  using Lookup = ParserAtomLookup;

  static inline HashNumber hash(const Lookup& l) { return l.hash(); }
  static inline bool match(const ParserAtom* entry, const Lookup& l) {
    return l.equalsEntry(entry);
  }
};

struct WellKnownAtomInfoHasher {
  using Lookup = ParserAtomLookup;

  static inline HashNumber hash(const Lookup& l) { return l.hash(); }
  static inline bool match(const WellKnownAtomInfo* info, const Lookup& l) {
    return l.equalsEntry(info);
  }
};

using ParserAtomVector = Vector<ParserAtom*, 0, js::SystemAllocPolicy>;
using ParserAtomSpan = mozilla::Span<ParserAtom*>;

/**
 * WellKnownParserAtoms allows the parser to lookup up specific atoms in
 * constant time.
 */
class WellKnownParserAtoms {
 public:
  // Common property and prototype names are tracked in a hash table. This table
  // does not key for any items already in a direct-indexing tiny atom table.
  using EntryMap = HashMap<const WellKnownAtomInfo*, TaggedParserAtomIndex,
                           WellKnownAtomInfoHasher, js::SystemAllocPolicy>;
  EntryMap wellKnownMap_;

  bool initSingle(JSContext* cx, const WellKnownAtomInfo& info,
                  TaggedParserAtomIndex index);

 public:
  bool init(JSContext* cx);

  // Maximum length of any well known atoms. This can be increased if needed.
  static constexpr size_t MaxWellKnownLength = 32;

  template <typename CharT>
  TaggedParserAtomIndex lookupChar16Seq(
      const SpecificParserAtomLookup<CharT>& lookup) const;

  template <typename CharsT>
  TaggedParserAtomIndex lookupTinyIndex(CharsT chars, size_t length) const {
    static_assert(std::is_same_v<CharsT, const Latin1Char*> ||
                      std::is_same_v<CharsT, const char16_t*> ||
                      std::is_same_v<CharsT, const char*> ||
                      std::is_same_v<CharsT, char16_t*> ||
                      std::is_same_v<CharsT, LittleEndianChars>,
                  "This assert mostly explicitly documents the calling types, "
                  "and forces that to be updated if new types show up.");
    switch (length) {
      case 0:
        return TaggedParserAtomIndex::WellKnown::empty();

      case 1: {
        if (char16_t(chars[0]) < TaggedParserAtomIndex::Length1StaticLimit) {
          return TaggedParserAtomIndex(Length1StaticParserString(chars[0]));
        }
        break;
      }

      case 2:
        if (StaticStrings::fitsInSmallChar(chars[0]) &&
            StaticStrings::fitsInSmallChar(chars[1])) {
          return TaggedParserAtomIndex(Length2StaticParserString(
              StaticStrings::getLength2Index(chars[0], chars[1])));
        }
        break;
    }

    // No match on tiny Atoms
    return TaggedParserAtomIndex::null();
  }

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(this) +
           wellKnownMap_.shallowSizeOfExcludingThis(mallocSizeOf);
  }
};

bool InstantiateMarkedAtoms(JSContext* cx, const ParserAtomSpan& entries,
                            CompilationAtomCache& atomCache);

/**
 * A ParserAtomsTable owns and manages the vector of ParserAtom entries
 * associated with a given compile session.
 */
class ParserAtomsTable {
  friend class BorrowingCompilationStencil;

 private:
  const WellKnownParserAtoms& wellKnownTable_;

  LifoAlloc* alloc_;

  // The ParserAtom are owned by the LifoAlloc.
  using EntryMap = HashMap<const ParserAtom*, TaggedParserAtomIndex,
                           ParserAtomLookupHasher, js::SystemAllocPolicy>;
  EntryMap entryMap_;
  ParserAtomVector entries_;

 public:
  ParserAtomsTable(JSRuntime* rt, LifoAlloc& alloc);
  ParserAtomsTable(ParserAtomsTable&&) = default;
  ParserAtomsTable& operator=(ParserAtomsTable&& other) noexcept {
    entryMap_ = std::move(other.entryMap_);
    entries_ = std::move(other.entries_);
    return *this;
  }

  void fixupAlloc(LifoAlloc& alloc) { alloc_ = &alloc; }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return entryMap_.shallowSizeOfExcludingThis(mallocSizeOf) +
           entries_.sizeOfExcludingThis(mallocSizeOf);
  }
  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(this) + sizeOfExcludingThis(mallocSizeOf);
  }

 private:
  // Internal APIs for interning to the table after well-known atoms cases have
  // been tested.
  TaggedParserAtomIndex addEntry(JSContext* cx, EntryMap::AddPtr& addPtr,
                                 ParserAtom* entry);
  template <typename AtomCharT, typename SeqCharT>
  TaggedParserAtomIndex internChar16Seq(JSContext* cx, EntryMap::AddPtr& addPtr,
                                        HashNumber hash,
                                        InflatedChar16Sequence<SeqCharT> seq,
                                        uint32_t length);

  template <typename AtomCharT>
  TaggedParserAtomIndex internExternalParserAtomImpl(JSContext* cx,
                                                     const ParserAtom* atom);

 public:
  TaggedParserAtomIndex internAscii(JSContext* cx, const char* asciiPtr,
                                    uint32_t length);

  TaggedParserAtomIndex internLatin1(JSContext* cx,
                                     const JS::Latin1Char* latin1Ptr,
                                     uint32_t length);

  TaggedParserAtomIndex internUtf8(JSContext* cx,
                                   const mozilla::Utf8Unit* utf8Ptr,
                                   uint32_t nbyte);

  TaggedParserAtomIndex internChar16(JSContext* cx, const char16_t* char16Ptr,
                                     uint32_t length);

  TaggedParserAtomIndex internJSAtom(JSContext* cx,
                                     CompilationAtomCache& atomCache,
                                     JSAtom* atom);

  TaggedParserAtomIndex internExternalParserAtom(JSContext* cx,
                                                 const ParserAtom* atom);

  bool addPlaceholder(JSContext* cx);

 private:
  const ParserAtom* getWellKnown(WellKnownAtomId atomId) const;
  ParserAtom* getParserAtom(ParserAtomIndex index) const;

 public:
  const ParserAtomVector& entries() const { return entries_; }

  // Accessors for querying atom properties.
  bool isIdentifier(TaggedParserAtomIndex index) const;
  bool isPrivateName(TaggedParserAtomIndex index) const;
  bool isExtendedUnclonedSelfHostedFunctionName(
      TaggedParserAtomIndex index) const;
  bool isModuleExportName(TaggedParserAtomIndex index) const;
  bool isIndex(TaggedParserAtomIndex index, uint32_t* indexp) const;
  uint32_t length(TaggedParserAtomIndex index) const;

  // Methods for atom.
  void markUsedByStencil(TaggedParserAtomIndex index) const;
  bool toNumber(JSContext* cx, TaggedParserAtomIndex index,
                double* result) const;
  UniqueChars toNewUTF8CharsZ(JSContext* cx, TaggedParserAtomIndex index) const;
  UniqueChars toPrintableString(JSContext* cx,
                                TaggedParserAtomIndex index) const;
  UniqueChars toQuotedString(JSContext* cx, TaggedParserAtomIndex index) const;
  JSAtom* toJSAtom(JSContext* cx, TaggedParserAtomIndex index,
                   CompilationAtomCache& atomCache) const;
  bool appendTo(StringBuffer& buffer, TaggedParserAtomIndex index) const;

 public:
#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump(TaggedParserAtomIndex index) const;
  void dumpCharsNoQuote(js::GenericPrinter& out,
                        TaggedParserAtomIndex index) const;

  static void dumpCharsNoQuote(js::GenericPrinter& out, WellKnownAtomId id);
  static void dumpCharsNoQuote(js::GenericPrinter& out,
                               Length1StaticParserString index);
  static void dumpCharsNoQuote(js::GenericPrinter& out,
                               Length2StaticParserString index);
#endif

  static void getLength1Content(Length1StaticParserString s, char contents[1]) {
    contents[0] = char(s);
  }

  static void getLength2Content(Length2StaticParserString s, char contents[2]) {
    contents[0] = StaticStrings::firstCharOfLength2(size_t(s));
    contents[1] = StaticStrings::secondCharOfLength2(size_t(s));
  }
};

// Lightweight version of ParserAtomsTable.
// This doesn't support deduplication.
// Used while decoding XDR.
class ParserAtomSpanBuilder {
  ParserAtomSpan& entries_;

 public:
  explicit ParserAtomSpanBuilder(ParserAtomSpan& entries) : entries_(entries) {}

  bool allocate(JSContext* cx, LifoAlloc& alloc, size_t count);

  void set(ParserAtomIndex index, const ParserAtom* atom) {
    entries_[index] = const_cast<ParserAtom*>(atom);
  }
};

template <typename CharT>
class SpecificParserAtomLookup : public ParserAtomLookup {
  // The sequence of characters to look up.
  InflatedChar16Sequence<CharT> seq_;

 public:
  explicit SpecificParserAtomLookup(const InflatedChar16Sequence<CharT>& seq)
      : SpecificParserAtomLookup(seq, seq.computeHash()) {}

  SpecificParserAtomLookup(const InflatedChar16Sequence<CharT>& seq,
                           HashNumber hash)
      : ParserAtomLookup(hash), seq_(seq) {
    MOZ_ASSERT(seq_.computeHash() == hash);
  }

  virtual bool equalsEntry(const ParserAtom* entry) const override {
    return entry->equalsSeq<CharT>(hash_, seq_);
  }

  virtual bool equalsEntry(const WellKnownAtomInfo* info) const override {
    // Compare hashes first.
    if (info->hash != hash_) {
      return false;
    }

    InflatedChar16Sequence<CharT> seq = seq_;
    for (uint32_t i = 0; i < info->length; i++) {
      if (!seq.hasMore() || char16_t(info->content[i]) != seq.next()) {
        return false;
      }
    }
    return !seq.hasMore();
  }
};

template <typename CharT>
inline bool ParserAtom::equalsSeq(HashNumber hash,
                                  InflatedChar16Sequence<CharT> seq) const {
  // Compare hashes first.
  if (hash_ != hash) {
    return false;
  }

  if (hasTwoByteChars()) {
    const char16_t* chars = twoByteChars();
    for (uint32_t i = 0; i < length_; i++) {
      if (!seq.hasMore() || chars[i] != seq.next()) {
        return false;
      }
    }
  } else {
    const Latin1Char* chars = latin1Chars();
    for (uint32_t i = 0; i < length_; i++) {
      if (!seq.hasMore() || char16_t(chars[i]) != seq.next()) {
        return false;
      }
    }
  }
  return !seq.hasMore();
}

JSAtom* GetWellKnownAtom(JSContext* cx, WellKnownAtomId atomId);

} /* namespace frontend */
} /* namespace js */

#endif  // frontend_ParserAtom_h
