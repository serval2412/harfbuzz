/*
 * Copyright © 2007,2008,2009,2010  Red Hat, Inc.
 * Copyright © 2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OPEN_TYPE_PRIVATE_HH
#define HB_OPEN_TYPE_PRIVATE_HH

#include "hb-private.hh"
#include "hb-blob-private.hh"
#include "hb-face-private.hh"


namespace OT {



/*
 * Casts
 */

/* Cast to struct T, reference to reference */
template<typename Type, typename TObject>
static inline const Type& CastR(const TObject &X)
{ return reinterpret_cast<const Type&> (X); }
template<typename Type, typename TObject>
static inline Type& CastR(TObject &X)
{ return reinterpret_cast<Type&> (X); }

/* Cast to struct T, pointer to pointer */
template<typename Type, typename TObject>
static inline const Type* CastP(const TObject *X)
{ return reinterpret_cast<const Type*> (X); }
template<typename Type, typename TObject>
static inline Type* CastP(TObject *X)
{ return reinterpret_cast<Type*> (X); }

/* StructAtOffset<T>(P,Ofs) returns the struct T& that is placed at memory
 * location pointed to by P plus Ofs bytes. */
template<typename Type>
static inline const Type& StructAtOffset(const void *P, unsigned int offset)
{ return * reinterpret_cast<const Type*> ((const char *) P + offset); }
template<typename Type>
static inline Type& StructAtOffset(void *P, unsigned int offset)
{ return * reinterpret_cast<Type*> ((char *) P + offset); }

/* StructAfter<T>(X) returns the struct T& that is placed after X.
 * Works with X of variable size also.  X must implement get_size() */
template<typename Type, typename TObject>
static inline const Type& StructAfter(const TObject &X)
{ return StructAtOffset<Type>(&X, X.get_size()); }
template<typename Type, typename TObject>
static inline Type& StructAfter(TObject &X)
{ return StructAtOffset<Type>(&X, X.get_size()); }



/*
 * Size checking
 */

/* Check _assertion in a method environment */
#define _DEFINE_INSTANCE_ASSERTION1(_line, _assertion) \
  inline void _instance_assertion_on_line_##_line (void) const \
  { \
    static_assert ((_assertion), ""); \
    ASSERT_INSTANCE_POD (*this); /* Make sure it's POD. */ \
  }
# define _DEFINE_INSTANCE_ASSERTION0(_line, _assertion) _DEFINE_INSTANCE_ASSERTION1 (_line, _assertion)
# define DEFINE_INSTANCE_ASSERTION(_assertion) _DEFINE_INSTANCE_ASSERTION0 (__LINE__, _assertion)

/* Check that _code compiles in a method environment */
#define _DEFINE_COMPILES_ASSERTION1(_line, _code) \
  inline void _compiles_assertion_on_line_##_line (void) const \
  { _code; }
# define _DEFINE_COMPILES_ASSERTION0(_line, _code) _DEFINE_COMPILES_ASSERTION1 (_line, _code)
# define DEFINE_COMPILES_ASSERTION(_code) _DEFINE_COMPILES_ASSERTION0 (__LINE__, _code)


#define DEFINE_SIZE_STATIC(size) \
  DEFINE_INSTANCE_ASSERTION (sizeof (*this) == (size)); \
  static const unsigned int static_size = (size); \
  static const unsigned int min_size = (size); \
  inline unsigned int get_size (void) const { return (size); }

#define DEFINE_SIZE_UNION(size, _member) \
  DEFINE_INSTANCE_ASSERTION (0*sizeof(this->u._member.static_size) + sizeof(this->u._member) == (size)); \
  static const unsigned int min_size = (size)

#define DEFINE_SIZE_MIN(size) \
  DEFINE_INSTANCE_ASSERTION (sizeof (*this) >= (size)); \
  static const unsigned int min_size = (size)

#define DEFINE_SIZE_ARRAY(size, array) \
  DEFINE_INSTANCE_ASSERTION (sizeof (*this) == (size) + sizeof (array[0])); \
  DEFINE_COMPILES_ASSERTION ((void) array[0].static_size) \
  static const unsigned int min_size = (size)

#define DEFINE_SIZE_ARRAY2(size, array1, array2) \
  DEFINE_INSTANCE_ASSERTION (sizeof (*this) == (size) + sizeof (this->array1[0]) + sizeof (this->array2[0])); \
  DEFINE_COMPILES_ASSERTION ((void) array1[0].static_size; (void) array2[0].static_size) \
  static const unsigned int min_size = (size)



/*
 * Dispatch
 */

template <typename Context, typename Return, unsigned int MaxDebugDepth>
struct hb_dispatch_context_t
{
  static const unsigned int max_debug_depth = MaxDebugDepth;
  typedef Return return_t;
  template <typename T, typename F>
  inline bool may_dispatch (const T *obj, const F *format) { return true; }
  static return_t no_dispatch_return_value (void) { return Context::default_return_value (); }
};


/*
 * Sanitize
 */

/* This limits sanitizing time on really broken fonts. */
#ifndef HB_SANITIZE_MAX_EDITS
#define HB_SANITIZE_MAX_EDITS 32
#endif
#ifndef HB_SANITIZE_MAX_OPS_FACTOR
#define HB_SANITIZE_MAX_OPS_FACTOR 8
#endif
#ifndef HB_SANITIZE_MAX_OPS_MIN
#define HB_SANITIZE_MAX_OPS_MIN 16384
#endif

struct hb_sanitize_context_t :
       hb_dispatch_context_t<hb_sanitize_context_t, bool, HB_DEBUG_SANITIZE>
{
  inline hb_sanitize_context_t (void) :
	debug_depth (0),
	start (nullptr), end (nullptr),
	writable (false), edit_count (0), max_ops (0),
	blob (nullptr),
	num_glyphs (0) {}

  inline const char *get_name (void) { return "SANITIZE"; }
  template <typename T, typename F>
  inline bool may_dispatch (const T *obj, const F *format)
  { return format->sanitize (this); }
  template <typename T>
  inline return_t dispatch (const T &obj) { return obj.sanitize (this); }
  static return_t default_return_value (void) { return true; }
  static return_t no_dispatch_return_value (void) { return false; }
  bool stop_sublookup_iteration (const return_t r) const { return !r; }

  inline void init (hb_blob_t *b)
  {
    this->blob = hb_blob_reference (b);
    this->writable = false;
  }

  inline void set_num_glyphs (unsigned int num_glyphs_) { num_glyphs = num_glyphs_; }
  inline unsigned int get_num_glyphs (void) { return num_glyphs; }

  inline void start_processing (void)
  {
    this->start = hb_blob_get_data (this->blob, nullptr);
    this->end = this->start + this->blob->length;
    assert (this->start <= this->end); /* Must not overflow. */
    this->max_ops = MAX ((unsigned int) (this->end - this->start) * HB_SANITIZE_MAX_OPS_FACTOR,
			 (unsigned) HB_SANITIZE_MAX_OPS_MIN);
    this->edit_count = 0;
    this->debug_depth = 0;

    DEBUG_MSG_LEVEL (SANITIZE, start, 0, +1,
		     "start [%p..%p] (%lu bytes)",
		     this->start, this->end,
		     (unsigned long) (this->end - this->start));
  }

  inline void end_processing (void)
  {
    DEBUG_MSG_LEVEL (SANITIZE, this->start, 0, -1,
		     "end [%p..%p] %u edit requests",
		     this->start, this->end, this->edit_count);

    hb_blob_destroy (this->blob);
    this->blob = nullptr;
    this->start = this->end = nullptr;
  }

  inline bool check_range (const void *base, unsigned int len) const
  {
    const char *p = (const char *) base;
    bool ok = this->max_ops-- > 0 &&
	      this->start <= p &&
	      p <= this->end &&
	      (unsigned int) (this->end - p) >= len;

    DEBUG_MSG_LEVEL (SANITIZE, p, this->debug_depth+1, 0,
       "check_range [%p..%p] (%d bytes) in [%p..%p] -> %s",
       p, p + len, len,
       this->start, this->end,
       ok ? "OK" : "OUT-OF-RANGE");

    return likely (ok);
  }

  inline bool check_array (const void *base, unsigned int record_size, unsigned int len) const
  {
    const char *p = (const char *) base;
    bool overflows = hb_unsigned_mul_overflows (len, record_size);
    unsigned int array_size = record_size * len;
    bool ok = !overflows && this->check_range (base, array_size);

    DEBUG_MSG_LEVEL (SANITIZE, p, this->debug_depth+1, 0,
       "check_array [%p..%p] (%d*%d=%d bytes) in [%p..%p] -> %s",
       p, p + (record_size * len), record_size, len, (unsigned int) array_size,
       this->start, this->end,
       overflows ? "OVERFLOWS" : ok ? "OK" : "OUT-OF-RANGE");

    return likely (ok);
  }

  template <typename Type>
  inline bool check_struct (const Type *obj) const
  {
    return likely (this->check_range (obj, obj->min_size));
  }

  inline bool may_edit (const void *base, unsigned int len)
  {
    if (this->edit_count >= HB_SANITIZE_MAX_EDITS)
      return false;

    const char *p = (const char *) base;
    this->edit_count++;

    DEBUG_MSG_LEVEL (SANITIZE, p, this->debug_depth+1, 0,
       "may_edit(%u) [%p..%p] (%d bytes) in [%p..%p] -> %s",
       this->edit_count,
       p, p + len, len,
       this->start, this->end,
       this->writable ? "GRANTED" : "DENIED");

    return this->writable;
  }

  template <typename Type, typename ValueType>
  inline bool try_set (const Type *obj, const ValueType &v) {
    if (this->may_edit (obj, obj->static_size)) {
      const_cast<Type *> (obj)->set (v);
      return true;
    }
    return false;
  }

  template <typename Type>
  inline hb_blob_t *sanitize_blob (hb_blob_t *blob)
  {
    bool sane;

    init (blob);

  retry:
    DEBUG_MSG_FUNC (SANITIZE, start, "start");

    start_processing ();

    if (unlikely (!start))
    {
      end_processing ();
      return blob;
    }

    Type *t = CastP<Type> (const_cast<char *> (start));

    sane = t->sanitize (this);
    if (sane)
    {
      if (edit_count)
      {
	DEBUG_MSG_FUNC (SANITIZE, start, "passed first round with %d edits; going for second round", edit_count);

        /* sanitize again to ensure no toe-stepping */
        edit_count = 0;
	sane = t->sanitize (this);
	if (edit_count) {
	  DEBUG_MSG_FUNC (SANITIZE, start, "requested %d edits in second round; FAILLING", edit_count);
	  sane = false;
	}
      }
    }
    else
    {
      if (edit_count && !writable) {
        start = hb_blob_get_data_writable (blob, nullptr);
	end = start + blob->length;

	if (start)
	{
	  writable = true;
	  /* ok, we made it writable by relocating.  try again */
	  DEBUG_MSG_FUNC (SANITIZE, start, "retry");
	  goto retry;
	}
      }
    }

    end_processing ();

    DEBUG_MSG_FUNC (SANITIZE, start, sane ? "PASSED" : "FAILED");
    if (sane)
    {
      blob->lock ();
      return blob;
    }
    else
    {
      hb_blob_destroy (blob);
      return hb_blob_get_empty ();
    }
  }

  template <typename Type>
  inline hb_blob_t *reference_table (const hb_face_t *face, hb_tag_t tableTag = Type::tableTag)
  {
    return sanitize_blob<Type> (face->reference_table (tableTag));
  }

  mutable unsigned int debug_depth;
  const char *start, *end;
  private:
  bool writable;
  unsigned int edit_count;
  mutable int max_ops;
  hb_blob_t *blob;
  unsigned int num_glyphs;
};


/*
 * Serialize
 */


struct hb_serialize_context_t
{
  inline hb_serialize_context_t (void *start_, unsigned int size)
  {
    this->start = (char *) start_;
    this->end = this->start + size;

    this->ran_out_of_room = false;
    this->head = this->start;
    this->debug_depth = 0;
  }

  template <typename Type>
  inline Type *start_serialize (void)
  {
    DEBUG_MSG_LEVEL (SERIALIZE, this->start, 0, +1,
		     "start [%p..%p] (%lu bytes)",
		     this->start, this->end,
		     (unsigned long) (this->end - this->start));

    return start_embed<Type> ();
  }

  inline void end_serialize (void)
  {
    DEBUG_MSG_LEVEL (SERIALIZE, this->start, 0, -1,
		     "end [%p..%p] serialized %d bytes; %s",
		     this->start, this->end,
		     (int) (this->head - this->start),
		     this->ran_out_of_room ? "RAN OUT OF ROOM" : "did not ran out of room");

  }

  template <typename Type>
  inline Type *copy (void)
  {
    assert (!this->ran_out_of_room);
    unsigned int len = this->head - this->start;
    void *p = malloc (len);
    if (p)
      memcpy (p, this->start, len);
    return reinterpret_cast<Type *> (p);
  }

  template <typename Type>
  inline Type *allocate_size (unsigned int size)
  {
    if (unlikely (this->ran_out_of_room || this->end - this->head < ptrdiff_t (size))) {
      this->ran_out_of_room = true;
      return nullptr;
    }
    memset (this->head, 0, size);
    char *ret = this->head;
    this->head += size;
    return reinterpret_cast<Type *> (ret);
  }

  template <typename Type>
  inline Type *allocate_min (void)
  {
    return this->allocate_size<Type> (Type::min_size);
  }

  template <typename Type>
  inline Type *start_embed (void)
  {
    Type *ret = reinterpret_cast<Type *> (this->head);
    return ret;
  }

  template <typename Type>
  inline Type *embed (const Type &obj)
  {
    unsigned int size = obj.get_size ();
    Type *ret = this->allocate_size<Type> (size);
    if (unlikely (!ret)) return nullptr;
    memcpy (ret, obj, size);
    return ret;
  }

  template <typename Type>
  inline Type *extend_min (Type &obj)
  {
    unsigned int size = obj.min_size;
    assert (this->start <= (char *) &obj && (char *) &obj <= this->head && (char *) &obj + size >= this->head);
    if (unlikely (!this->allocate_size<Type> (((char *) &obj) + size - this->head))) return nullptr;
    return reinterpret_cast<Type *> (&obj);
  }

  template <typename Type>
  inline Type *extend (Type &obj)
  {
    unsigned int size = obj.get_size ();
    assert (this->start < (char *) &obj && (char *) &obj <= this->head && (char *) &obj + size >= this->head);
    if (unlikely (!this->allocate_size<Type> (((char *) &obj) + size - this->head))) return nullptr;
    return reinterpret_cast<Type *> (&obj);
  }

  inline void truncate (void *new_head)
  {
    assert (this->start < new_head && new_head <= this->head);
    this->head = (char *) new_head;
  }

  unsigned int debug_depth;
  char *start, *end, *head;
  bool ran_out_of_room;
};

template <typename Type>
struct Supplier
{
  inline Supplier (const Type *array, unsigned int len_, unsigned int stride_=sizeof(Type))
  {
    head = array;
    len = len_;
    stride = stride_;
  }
  inline const Type operator [] (unsigned int i) const
  {
    if (unlikely (i >= len)) return Type ();
    return * (const Type *) (const void *) ((const char *) head + stride * i);
  }

  inline Supplier<Type> & operator += (unsigned int count)
  {
    if (unlikely (count > len))
      count = len;
    len -= count;
    head = (const Type *) (const void *) ((const char *) head + stride * count);
    return *this;
  }

  private:
  inline Supplier (const Supplier<Type> &); /* Disallow copy */
  inline Supplier<Type>& operator= (const Supplier<Type> &); /* Disallow copy */

  unsigned int len;
  unsigned int stride;
  const Type *head;
};


/*
 *
 * The OpenType Font File: Data Types
 */


/* "The following data types are used in the OpenType font file.
 *  All OpenType fonts use Motorola-style byte ordering (Big Endian):" */

/*
 * Int types
 */


template <typename Type, int Bytes> struct BEInt;

template <typename Type>
struct BEInt<Type, 1>
{
  public:
  inline void set (Type V)
  {
    v = V;
  }
  inline operator Type (void) const
  {
    return v;
  }
  private: uint8_t v;
};
template <typename Type>
struct BEInt<Type, 2>
{
  public:
  inline void set (Type V)
  {
    v[0] = (V >>  8) & 0xFF;
    v[1] = (V      ) & 0xFF;
  }
  inline operator Type (void) const
  {
    return (v[0] <<  8)
         + (v[1]      );
  }
  private: uint8_t v[2];
};
template <typename Type>
struct BEInt<Type, 3>
{
  public:
  inline void set (Type V)
  {
    v[0] = (V >> 16) & 0xFF;
    v[1] = (V >>  8) & 0xFF;
    v[2] = (V      ) & 0xFF;
  }
  inline operator Type (void) const
  {
    return (v[0] << 16)
         + (v[1] <<  8)
         + (v[2]      );
  }
  private: uint8_t v[3];
};
template <typename Type>
struct BEInt<Type, 4>
{
  public:
  inline void set (Type V)
  {
    v[0] = (V >> 24) & 0xFF;
    v[1] = (V >> 16) & 0xFF;
    v[2] = (V >>  8) & 0xFF;
    v[3] = (V      ) & 0xFF;
  }
  inline operator Type (void) const
  {
    return (v[0] << 24)
         + (v[1] << 16)
         + (v[2] <<  8)
         + (v[3]      );
  }
  private: uint8_t v[4];
};

/* Integer types in big-endian order and no alignment requirement */
template <typename Type, unsigned int Size>
struct IntType
{
  inline void set (Type i) { v.set (i); }
  inline operator Type(void) const { return v; }
  inline bool operator == (const IntType<Type,Size> &o) const { return (Type) v == (Type) o.v; }
  inline bool operator != (const IntType<Type,Size> &o) const { return !(*this == o); }
  static inline int cmp (const IntType<Type,Size> *a, const IntType<Type,Size> *b) { return b->cmp (*a); }
  template <typename Type2>
  inline int cmp (Type2 a) const
  {
    Type b = v;
    if (sizeof (Type) < sizeof (int) && sizeof (Type2) < sizeof (int))
      return (int) a - (int) b;
    else
      return a < b ? -1 : a == b ? 0 : +1;
  }
  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this)));
  }
  protected:
  BEInt<Type, Size> v;
  public:
  DEFINE_SIZE_STATIC (Size);
};

typedef IntType<uint8_t,  1> HBUINT8;	/* 8-bit unsigned integer. */
typedef IntType<int8_t,   1> HBINT8;	/* 8-bit signed integer. */
typedef IntType<uint16_t, 2> HBUINT16;	/* 16-bit unsigned integer. */
typedef IntType<int16_t,  2> HBINT16;	/* 16-bit signed integer. */
typedef IntType<uint32_t, 4> HBUINT32;	/* 32-bit unsigned integer. */
typedef IntType<int32_t,  4> HBINT32;	/* 32-bit signed integer. */
typedef IntType<uint32_t, 3> HBUINT24;	/* 24-bit unsigned integer. */

/* 16-bit signed integer (HBINT16) that describes a quantity in FUnits. */
typedef HBINT16 FWORD;

/* 16-bit unsigned integer (HBUINT16) that describes a quantity in FUnits. */
typedef HBUINT16 UFWORD;

/* 16-bit signed fixed number with the low 14 bits of fraction (2.14). */
struct F2DOT14 : HBINT16
{
  // 16384 means 1<<14
  inline float to_float (void) const { return ((int32_t) v) / 16384.f; }
  inline void set_float (float f) { v.set (round (f * 16384.f)); }
  public:
  DEFINE_SIZE_STATIC (2);
};

/* 32-bit signed fixed-point number (16.16). */
struct Fixed : HBINT32
{
  // 65536 means 1<<16
  inline float to_float (void) const { return ((int32_t) v) / 65536.f; }
  inline void set_float (float f) { v.set (round (f * 65536.f)); }
  public:
  DEFINE_SIZE_STATIC (4);
};

/* Date represented in number of seconds since 12:00 midnight, January 1,
 * 1904. The value is represented as a signed 64-bit integer. */
struct LONGDATETIME
{
  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this)));
  }
  protected:
  HBINT32 major;
  HBUINT32 minor;
  public:
  DEFINE_SIZE_STATIC (8);
};

/* Array of four uint8s (length = 32 bits) used to identify a script, language
 * system, feature, or baseline */
struct Tag : HBUINT32
{
  /* What the char* converters return is NOT nul-terminated.  Print using "%.4s" */
  inline operator const char* (void) const { return reinterpret_cast<const char *> (&this->v); }
  inline operator char* (void) { return reinterpret_cast<char *> (&this->v); }
  public:
  DEFINE_SIZE_STATIC (4);
};
DEFINE_NULL_DATA (OT, Tag, "    ");

/* Glyph index number, same as uint16 (length = 16 bits) */
typedef HBUINT16 GlyphID;

/* Name-table index, same as uint16 (length = 16 bits) */
typedef HBUINT16 NameID;

/* Script/language-system/feature index */
struct Index : HBUINT16 {
  static const unsigned int NOT_FOUND_INDEX = 0xFFFFu;
};
DEFINE_NULL_DATA (OT, Index, "\xff\xff");

/* Offset, Null offset = 0 */
template <typename Type>
struct Offset : Type
{
  inline bool is_null (void) const { return 0 == *this; }

  inline void *serialize (hb_serialize_context_t *c, const void *base)
  {
    void *t = c->start_embed<void> ();
    this->set ((char *) t - (char *) base); /* TODO(serialize) Overflow? */
    return t;
  }

  public:
  DEFINE_SIZE_STATIC (sizeof(Type));
};

typedef Offset<HBUINT16> Offset16;
typedef Offset<HBUINT32> Offset32;


/* CheckSum */
struct CheckSum : HBUINT32
{
  /* This is reference implementation from the spec. */
  static inline uint32_t CalcTableChecksum (const HBUINT32 *Table, uint32_t Length)
  {
    uint32_t Sum = 0L;
    assert (0 == (Length & 3));
    const HBUINT32 *EndPtr = Table + Length / HBUINT32::static_size;

    while (Table < EndPtr)
      Sum += *Table++;
    return Sum;
  }

  /* Note: data should be 4byte aligned and have 4byte padding at the end. */
  inline void set_for_data (const void *data, unsigned int length)
  { set (CalcTableChecksum ((const HBUINT32 *) data, length)); }

  public:
  DEFINE_SIZE_STATIC (4);
};


/*
 * Version Numbers
 */

template <typename FixedType=HBUINT16>
struct FixedVersion
{
  inline uint32_t to_int (void) const { return (major << (sizeof(FixedType) * 8)) + minor; }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  FixedType major;
  FixedType minor;
  public:
  DEFINE_SIZE_STATIC (2 * sizeof(FixedType));
};



/*
 * Template subclasses of Offset that do the dereferencing.
 * Use: (base+offset)
 */

template <typename Type, typename OffsetType=HBUINT16>
struct OffsetTo : Offset<OffsetType>
{
  inline const Type& operator () (const void *base) const
  {
    unsigned int offset = *this;
    if (unlikely (!offset)) return Null(Type);
    return StructAtOffset<const Type> (base, offset);
  }
  inline Type& operator () (void *base) const
  {
    unsigned int offset = *this;
    if (unlikely (!offset)) return Crap(Type);
    return StructAtOffset<Type> (base, offset);
  }

  inline Type& serialize (hb_serialize_context_t *c, const void *base)
  {
    return * (Type *) Offset<OffsetType>::serialize (c, base);
  }

  inline bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!c->check_struct (this))) return_trace (false);
    unsigned int offset = *this;
    if (unlikely (!offset)) return_trace (true);
    if (unlikely (!c->check_range (base, offset))) return_trace (false);
    const Type &obj = StructAtOffset<Type> (base, offset);
    return_trace (likely (obj.sanitize (c)) || neuter (c));
  }
  template <typename T>
  inline bool sanitize (hb_sanitize_context_t *c, const void *base, T user_data) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!c->check_struct (this))) return_trace (false);
    unsigned int offset = *this;
    if (unlikely (!offset)) return_trace (true);
    if (unlikely (!c->check_range (base, offset))) return_trace (false);
    const Type &obj = StructAtOffset<Type> (base, offset);
    return_trace (likely (obj.sanitize (c, user_data)) || neuter (c));
  }

  /* Set the offset to Null */
  inline bool neuter (hb_sanitize_context_t *c) const {
    return c->try_set (this, 0);
  }
  DEFINE_SIZE_STATIC (sizeof(OffsetType));
};
template <typename Type> struct LOffsetTo : OffsetTo<Type, HBUINT32> {};
template <typename Base, typename OffsetType, typename Type>
static inline const Type& operator + (const Base &base, const OffsetTo<Type, OffsetType> &offset) { return offset (base); }
template <typename Base, typename OffsetType, typename Type>
static inline Type& operator + (Base &base, OffsetTo<Type, OffsetType> &offset) { return offset (base); }


/*
 * Array Types
 */


/* TODO Use it in ArrayOf, HeadlessArrayOf, and other places around the code base?? */
template <typename Type>
struct UnsizedArrayOf
{
  inline const Type& operator [] (unsigned int i) const { return arrayZ[i]; }
  inline Type& operator [] (unsigned int i) { return arrayZ[i]; }

  inline bool sanitize (hb_sanitize_context_t *c, unsigned int count) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!sanitize_shallow (c, count))) return_trace (false);

    /* Note: for structs that do not reference other structs,
     * we do not need to call their sanitize() as we already did
     * a bound check on the aggregate array size.  We just include
     * a small unreachable expression to make sure the structs
     * pointed to do have a simple sanitize(), ie. they do not
     * reference other structs via offsets.
     */
    (void) (false && arrayZ[0].sanitize (c));

    return_trace (true);
  }
  inline bool sanitize (hb_sanitize_context_t *c, unsigned int count, const void *base) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!sanitize_shallow (c, count))) return_trace (false);
    for (unsigned int i = 0; i < count; i++)
      if (unlikely (!arrayZ[i].sanitize (c, base)))
        return_trace (false);
    return_trace (true);
  }
  template <typename T>
  inline bool sanitize (hb_sanitize_context_t *c, unsigned int count, const void *base, T user_data) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!sanitize_shallow (c, count))) return_trace (false);
    for (unsigned int i = 0; i < count; i++)
      if (unlikely (!arrayZ[i].sanitize (c, base, user_data)))
        return_trace (false);
    return_trace (true);
  }

  inline bool sanitize_shallow (hb_sanitize_context_t *c, unsigned int count) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_array (arrayZ, arrayZ[0].static_size, count));
  }

  public:
  Type	arrayZ[VAR];
  public:
  DEFINE_SIZE_ARRAY (0, arrayZ);
};

/* Unsized array of offset's */
template <typename Type, typename OffsetType>
struct UnsizedOffsetArrayOf : UnsizedArrayOf<OffsetTo<Type, OffsetType> > {};

/* Unsized array of offsets relative to the beginning of the array itself. */
template <typename Type, typename OffsetType>
struct UnsizedOffsetListOf : UnsizedOffsetArrayOf<Type, OffsetType>
{
  inline const Type& operator [] (unsigned int i) const
  {
    return this+this->arrayZ[i];
  }

  inline bool sanitize (hb_sanitize_context_t *c, unsigned int count) const
  {
    TRACE_SANITIZE (this);
    return_trace ((UnsizedOffsetArrayOf<Type, OffsetType>::sanitize (c, count, this)));
  }
  template <typename T>
  inline bool sanitize (hb_sanitize_context_t *c, unsigned int count, T user_data) const
  {
    TRACE_SANITIZE (this);
    return_trace ((UnsizedOffsetArrayOf<Type, OffsetType>::sanitize (c, count, this, user_data)));
  }
};


/* An array with a number of elements. */
template <typename Type, typename LenType=HBUINT16>
struct ArrayOf
{
  const Type *sub_array (unsigned int start_offset, unsigned int *pcount /* IN/OUT */) const
  {
    unsigned int count = len;
    if (unlikely (start_offset > count))
      count = 0;
    else
      count -= start_offset;
    count = MIN (count, *pcount);
    *pcount = count;
    return arrayZ + start_offset;
  }

  inline const Type& operator [] (unsigned int i) const
  {
    if (unlikely (i >= len)) return Null(Type);
    return arrayZ[i];
  }
  inline Type& operator [] (unsigned int i)
  {
    if (unlikely (i >= len)) return Crap(Type);
    return arrayZ[i];
  }
  inline unsigned int get_size (void) const
  { return len.static_size + len * Type::static_size; }

  inline bool serialize (hb_serialize_context_t *c,
			 unsigned int items_len)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (!c->extend_min (*this))) return_trace (false);
    len.set (items_len); /* TODO(serialize) Overflow? */
    if (unlikely (!c->extend (*this))) return_trace (false);
    return_trace (true);
  }

  inline bool serialize (hb_serialize_context_t *c,
			 Supplier<Type> &items,
			 unsigned int items_len)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (!serialize (c, items_len))) return_trace (false);
    for (unsigned int i = 0; i < items_len; i++)
      arrayZ[i] = items[i];
    items += items_len;
    return_trace (true);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!sanitize_shallow (c))) return_trace (false);

    /* Note: for structs that do not reference other structs,
     * we do not need to call their sanitize() as we already did
     * a bound check on the aggregate array size.  We just include
     * a small unreachable expression to make sure the structs
     * pointed to do have a simple sanitize(), ie. they do not
     * reference other structs via offsets.
     */
    (void) (false && arrayZ[0].sanitize (c));

    return_trace (true);
  }
  inline bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!sanitize_shallow (c))) return_trace (false);
    unsigned int count = len;
    for (unsigned int i = 0; i < count; i++)
      if (unlikely (!arrayZ[i].sanitize (c, base)))
        return_trace (false);
    return_trace (true);
  }
  template <typename T>
  inline bool sanitize (hb_sanitize_context_t *c, const void *base, T user_data) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!sanitize_shallow (c))) return_trace (false);
    unsigned int count = len;
    for (unsigned int i = 0; i < count; i++)
      if (unlikely (!arrayZ[i].sanitize (c, base, user_data)))
        return_trace (false);
    return_trace (true);
  }

  template <typename SearchType>
  inline int lsearch (const SearchType &x) const
  {
    unsigned int count = len;
    for (unsigned int i = 0; i < count; i++)
      if (!this->arrayZ[i].cmp (x))
        return i;
    return -1;
  }

  inline void qsort (void)
  {
    ::qsort (arrayZ, len, sizeof (Type), Type::cmp);
  }

  private:
  inline bool sanitize_shallow (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (len.sanitize (c) && c->check_array (arrayZ, Type::static_size, len));
  }

  public:
  LenType len;
  Type arrayZ[VAR];
  public:
  DEFINE_SIZE_ARRAY (sizeof (LenType), arrayZ);
};
template <typename Type> struct LArrayOf : ArrayOf<Type, HBUINT32> {};
typedef ArrayOf<HBUINT8, HBUINT8> PString;

/* Array of Offset's */
template <typename Type, typename OffsetType=HBUINT16>
struct OffsetArrayOf : ArrayOf<OffsetTo<Type, OffsetType> > {};

/* Array of offsets relative to the beginning of the array itself. */
template <typename Type>
struct OffsetListOf : OffsetArrayOf<Type>
{
  inline const Type& operator [] (unsigned int i) const
  {
    if (unlikely (i >= this->len)) return Null(Type);
    return this+this->arrayZ[i];
  }
  inline const Type& operator [] (unsigned int i)
  {
    if (unlikely (i >= this->len)) return Crap(Type);
    return this+this->arrayZ[i];
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (OffsetArrayOf<Type>::sanitize (c, this));
  }
  template <typename T>
  inline bool sanitize (hb_sanitize_context_t *c, T user_data) const
  {
    TRACE_SANITIZE (this);
    return_trace (OffsetArrayOf<Type>::sanitize (c, this, user_data));
  }
};


/* An array starting at second element. */
template <typename Type, typename LenType=HBUINT16>
struct HeadlessArrayOf
{
  inline const Type& operator [] (unsigned int i) const
  {
    if (unlikely (i >= len || !i)) return Null(Type);
    return arrayZ[i-1];
  }
  inline Type& operator [] (unsigned int i)
  {
    if (unlikely (i >= len || !i)) return Crap(Type);
    return arrayZ[i-1];
  }
  inline unsigned int get_size (void) const
  { return len.static_size + (len ? len - 1 : 0) * Type::static_size; }

  inline bool serialize (hb_serialize_context_t *c,
			 Supplier<Type> &items,
			 unsigned int items_len)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (!c->extend_min (*this))) return_trace (false);
    len.set (items_len); /* TODO(serialize) Overflow? */
    if (unlikely (!items_len)) return_trace (true);
    if (unlikely (!c->extend (*this))) return_trace (false);
    for (unsigned int i = 0; i < items_len - 1; i++)
      arrayZ[i] = items[i];
    items += items_len - 1;
    return_trace (true);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!sanitize_shallow (c))) return_trace (false);

    /* Note: for structs that do not reference other structs,
     * we do not need to call their sanitize() as we already did
     * a bound check on the aggregate array size.  We just include
     * a small unreachable expression to make sure the structs
     * pointed to do have a simple sanitize(), ie. they do not
     * reference other structs via offsets.
     */
    (void) (false && arrayZ[0].sanitize (c));

    return_trace (true);
  }

  private:
  inline bool sanitize_shallow (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (len.sanitize (c) &&
		  (!len || c->check_array (arrayZ, Type::static_size, len - 1)));
  }

  public:
  LenType len;
  Type arrayZ[VAR];
  public:
  DEFINE_SIZE_ARRAY (sizeof (LenType), arrayZ);
};


/*
 * An array with sorted elements.  Supports binary searching.
 */
template <typename Type, typename LenType=HBUINT16>
struct SortedArrayOf : ArrayOf<Type, LenType>
{
  template <typename SearchType>
  inline int bsearch (const SearchType &x) const
  {
    /* Hand-coded bsearch here since this is in the hot inner loop. */
    const Type *arr = this->arrayZ;
    int min = 0, max = (int) this->len - 1;
    while (min <= max)
    {
      int mid = (min + max) / 2;
      int c = arr[mid].cmp (x);
      if (c < 0)
        max = mid - 1;
      else if (c > 0)
        min = mid + 1;
      else
        return mid;
    }
    return -1;
  }
};

/*
 * Binary-search arrays
 */

struct BinSearchHeader
{
  inline operator uint32_t (void) const { return len; }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  inline void set (unsigned int v)
  {
    len.set (v);
    assert (len == v);
    entrySelector.set (MAX (1u, hb_bit_storage (v)) - 1);
    searchRange.set (16 * (1u << entrySelector));
    rangeShift.set (v * 16 > searchRange
		    ? 16 * v - searchRange
		    : 0);
  }

  protected:
  HBUINT16	len;
  HBUINT16	searchRange;
  HBUINT16	entrySelector;
  HBUINT16	rangeShift;

  public:
  DEFINE_SIZE_STATIC (8);
};

template <typename Type>
struct BinSearchArrayOf : SortedArrayOf<Type, BinSearchHeader> {};


/* Lazy struct and blob loaders. */

/* Logic is shared between hb_lazy_loader_t and hb_table_lazy_loader_t */
template <typename T>
struct hb_lazy_loader_t
{
  inline void init (hb_face_t *face_)
  {
    face = face_;
    instance = nullptr;
  }

  inline void fini (void)
  {
    if (instance && instance != &Null(T))
    {
      instance->fini();
      free (instance);
    }
  }

  inline const T* get (void) const
  {
  retry:
    T *p = (T *) hb_atomic_ptr_get (&instance);
    if (unlikely (!p))
    {
      p = (T *) calloc (1, sizeof (T));
      if (unlikely (!p))
        p = const_cast<T *> (&Null(T));
      else
	p->init (face);
      if (unlikely (!hb_atomic_ptr_cmpexch (const_cast<T **>(&instance), nullptr, p)))
      {
	if (p != &Null(T))
	  p->fini ();
	goto retry;
      }
    }
    return p;
  }

  inline const T* operator-> (void) const
  {
    return get ();
  }

  private:
  hb_face_t *face;
  mutable T *instance;
};

/* Logic is shared between hb_lazy_loader_t and hb_table_lazy_loader_t */
template <typename T>
struct hb_table_lazy_loader_t
{
  inline void init (hb_face_t *face_)
  {
    face = face_;
    blob = nullptr;
  }

  inline void fini (void)
  {
    hb_blob_destroy (blob);
  }

  inline hb_blob_t* get_blob (void) const
  {
  retry:
    hb_blob_t *b = (hb_blob_t *) hb_atomic_ptr_get (&blob);
    if (unlikely (!b))
    {
      b = OT::hb_sanitize_context_t().reference_table<T> (face);
      if (!hb_atomic_ptr_cmpexch (&blob, nullptr, b))
      {
	hb_blob_destroy (b);
	goto retry;
      }
      blob = b;
    }
    return b;
  }

  inline const T* get (void) const
  {
    hb_blob_t *b = get_blob ();
    return b->as<T> ();
  }

  inline const T* operator-> (void) const
  {
    return get();
  }

  private:
  hb_face_t *face;
  mutable hb_blob_t *blob;
};


} /* namespace OT */


#endif /* HB_OPEN_TYPE_PRIVATE_HH */
