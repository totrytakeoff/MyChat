// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: test_message.proto
// Protobuf C++ Version: 5.29.3

#ifndef test_5fmessage_2eproto_2epb_2eh
#define test_5fmessage_2eproto_2epb_2eh

#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/runtime_version.h"
#if PROTOBUF_VERSION != 5029003
#error "Protobuf C++ gencode is built with an incompatible version of"
#error "Protobuf C++ headers/runtime. See"
#error "https://protobuf.dev/support/cross-version-runtime-guarantee/#cpp"
#endif
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/arenastring.h"
#include "google/protobuf/generated_message_tctable_decl.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/metadata_lite.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_test_5fmessage_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_test_5fmessage_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_test_5fmessage_2eproto;
namespace test {
class AnotherTestMessage;
struct AnotherTestMessageDefaultTypeInternal;
extern AnotherTestMessageDefaultTypeInternal _AnotherTestMessage_default_instance_;
class TestMessage;
struct TestMessageDefaultTypeInternal;
extern TestMessageDefaultTypeInternal _TestMessage_default_instance_;
}  // namespace test
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

namespace test {

// ===================================================================


// -------------------------------------------------------------------

class TestMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.TestMessage) */ {
 public:
  inline TestMessage() : TestMessage(nullptr) {}
  ~TestMessage() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(TestMessage* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(TestMessage));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR TestMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline TestMessage(const TestMessage& from) : TestMessage(nullptr, from) {}
  inline TestMessage(TestMessage&& from) noexcept
      : TestMessage(nullptr, std::move(from)) {}
  inline TestMessage& operator=(const TestMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline TestMessage& operator=(TestMessage&& from) noexcept {
    if (this == &from) return *this;
    if (::google::protobuf::internal::CanMoveWithInternalSwap(GetArena(), from.GetArena())) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const TestMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const TestMessage* internal_default_instance() {
    return reinterpret_cast<const TestMessage*>(
        &_TestMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(TestMessage& a, TestMessage& b) { a.Swap(&b); }
  inline void Swap(TestMessage* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(TestMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  TestMessage* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<TestMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const TestMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const TestMessage& from) { TestMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() PROTOBUF_FINAL;
  #if defined(PROTOBUF_CUSTOM_VTABLE)
  private:
  static ::size_t ByteSizeLong(const ::google::protobuf::MessageLite& msg);
  static ::uint8_t* _InternalSerialize(
      const MessageLite& msg, ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream);

  public:
  ::size_t ByteSizeLong() const { return ByteSizeLong(*this); }
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const {
    return _InternalSerialize(*this, target, stream);
  }
  #else   // PROTOBUF_CUSTOM_VTABLE
  ::size_t ByteSizeLong() const final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  #endif  // PROTOBUF_CUSTOM_VTABLE
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  static void SharedDtor(MessageLite& self);
  void InternalSwap(TestMessage* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "test.TestMessage"; }

 protected:
  explicit TestMessage(::google::protobuf::Arena* arena);
  TestMessage(::google::protobuf::Arena* arena, const TestMessage& from);
  TestMessage(::google::protobuf::Arena* arena, TestMessage&& from) noexcept
      : TestMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::internal::ClassData* GetClassData() const PROTOBUF_FINAL;
  static void* PlacementNew_(const void*, void* mem,
                             ::google::protobuf::Arena* arena);
  static constexpr auto InternalNewImpl_();
  static const ::google::protobuf::internal::ClassDataFull _class_data_;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kTagsFieldNumber = 4,
    kNameFieldNumber = 2,
    kDataFieldNumber = 6,
    kValueFieldNumber = 3,
    kIdFieldNumber = 1,
    kIsValidFieldNumber = 5,
  };
  // repeated string tags = 4;
  int tags_size() const;
  private:
  int _internal_tags_size() const;

  public:
  void clear_tags() ;
  const std::string& tags(int index) const;
  std::string* mutable_tags(int index);
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_tags(int index, Arg_&& value, Args_... args);
  std::string* add_tags();
  template <typename Arg_ = const std::string&, typename... Args_>
  void add_tags(Arg_&& value, Args_... args);
  const ::google::protobuf::RepeatedPtrField<std::string>& tags() const;
  ::google::protobuf::RepeatedPtrField<std::string>* mutable_tags();

  private:
  const ::google::protobuf::RepeatedPtrField<std::string>& _internal_tags() const;
  ::google::protobuf::RepeatedPtrField<std::string>* _internal_mutable_tags();

  public:
  // string name = 2;
  void clear_name() ;
  const std::string& name() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_name(Arg_&& arg, Args_... args);
  std::string* mutable_name();
  PROTOBUF_NODISCARD std::string* release_name();
  void set_allocated_name(std::string* value);

  private:
  const std::string& _internal_name() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_name(
      const std::string& value);
  std::string* _internal_mutable_name();

  public:
  // string data = 6;
  void clear_data() ;
  const std::string& data() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_data(Arg_&& arg, Args_... args);
  std::string* mutable_data();
  PROTOBUF_NODISCARD std::string* release_data();
  void set_allocated_data(std::string* value);

  private:
  const std::string& _internal_data() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_data(
      const std::string& value);
  std::string* _internal_mutable_data();

  public:
  // double value = 3;
  void clear_value() ;
  double value() const;
  void set_value(double value);

  private:
  double _internal_value() const;
  void _internal_set_value(double value);

  public:
  // int32 id = 1;
  void clear_id() ;
  ::int32_t id() const;
  void set_id(::int32_t value);

  private:
  ::int32_t _internal_id() const;
  void _internal_set_id(::int32_t value);

  public:
  // bool is_valid = 5;
  void clear_is_valid() ;
  bool is_valid() const;
  void set_is_valid(bool value);

  private:
  bool _internal_is_valid() const;
  void _internal_set_is_valid(bool value);

  public:
  // @@protoc_insertion_point(class_scope:test.TestMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      3, 6, 0,
      37, 2>
      _table_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(
        ::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena);
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const TestMessage& from_msg);
    ::google::protobuf::RepeatedPtrField<std::string> tags_;
    ::google::protobuf::internal::ArenaStringPtr name_;
    ::google::protobuf::internal::ArenaStringPtr data_;
    double value_;
    ::int32_t id_;
    bool is_valid_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_5fmessage_2eproto;
};
// -------------------------------------------------------------------

class AnotherTestMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.AnotherTestMessage) */ {
 public:
  inline AnotherTestMessage() : AnotherTestMessage(nullptr) {}
  ~AnotherTestMessage() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(AnotherTestMessage* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(AnotherTestMessage));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR AnotherTestMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline AnotherTestMessage(const AnotherTestMessage& from) : AnotherTestMessage(nullptr, from) {}
  inline AnotherTestMessage(AnotherTestMessage&& from) noexcept
      : AnotherTestMessage(nullptr, std::move(from)) {}
  inline AnotherTestMessage& operator=(const AnotherTestMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline AnotherTestMessage& operator=(AnotherTestMessage&& from) noexcept {
    if (this == &from) return *this;
    if (::google::protobuf::internal::CanMoveWithInternalSwap(GetArena(), from.GetArena())) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const AnotherTestMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const AnotherTestMessage* internal_default_instance() {
    return reinterpret_cast<const AnotherTestMessage*>(
        &_AnotherTestMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(AnotherTestMessage& a, AnotherTestMessage& b) { a.Swap(&b); }
  inline void Swap(AnotherTestMessage* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(AnotherTestMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  AnotherTestMessage* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<AnotherTestMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const AnotherTestMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const AnotherTestMessage& from) { AnotherTestMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() PROTOBUF_FINAL;
  #if defined(PROTOBUF_CUSTOM_VTABLE)
  private:
  static ::size_t ByteSizeLong(const ::google::protobuf::MessageLite& msg);
  static ::uint8_t* _InternalSerialize(
      const MessageLite& msg, ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream);

  public:
  ::size_t ByteSizeLong() const { return ByteSizeLong(*this); }
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const {
    return _InternalSerialize(*this, target, stream);
  }
  #else   // PROTOBUF_CUSTOM_VTABLE
  ::size_t ByteSizeLong() const final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  #endif  // PROTOBUF_CUSTOM_VTABLE
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  static void SharedDtor(MessageLite& self);
  void InternalSwap(AnotherTestMessage* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "test.AnotherTestMessage"; }

 protected:
  explicit AnotherTestMessage(::google::protobuf::Arena* arena);
  AnotherTestMessage(::google::protobuf::Arena* arena, const AnotherTestMessage& from);
  AnotherTestMessage(::google::protobuf::Arena* arena, AnotherTestMessage&& from) noexcept
      : AnotherTestMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::internal::ClassData* GetClassData() const PROTOBUF_FINAL;
  static void* PlacementNew_(const void*, void* mem,
                             ::google::protobuf::Arena* arena);
  static constexpr auto InternalNewImpl_();
  static const ::google::protobuf::internal::ClassDataFull _class_data_;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kDescriptionFieldNumber = 2,
    kTimestampFieldNumber = 1,
  };
  // string description = 2;
  void clear_description() ;
  const std::string& description() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_description(Arg_&& arg, Args_... args);
  std::string* mutable_description();
  PROTOBUF_NODISCARD std::string* release_description();
  void set_allocated_description(std::string* value);

  private:
  const std::string& _internal_description() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_description(
      const std::string& value);
  std::string* _internal_mutable_description();

  public:
  // int64 timestamp = 1;
  void clear_timestamp() ;
  ::int64_t timestamp() const;
  void set_timestamp(::int64_t value);

  private:
  ::int64_t _internal_timestamp() const;
  void _internal_set_timestamp(::int64_t value);

  public:
  // @@protoc_insertion_point(class_scope:test.AnotherTestMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 0,
      43, 2>
      _table_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(
        ::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena);
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const AnotherTestMessage& from_msg);
    ::google::protobuf::internal::ArenaStringPtr description_;
    ::int64_t timestamp_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_5fmessage_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// TestMessage

// int32 id = 1;
inline void TestMessage::clear_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = 0;
}
inline ::int32_t TestMessage::id() const {
  // @@protoc_insertion_point(field_get:test.TestMessage.id)
  return _internal_id();
}
inline void TestMessage::set_id(::int32_t value) {
  _internal_set_id(value);
  // @@protoc_insertion_point(field_set:test.TestMessage.id)
}
inline ::int32_t TestMessage::_internal_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.id_;
}
inline void TestMessage::_internal_set_id(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = value;
}

// string name = 2;
inline void TestMessage::clear_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.ClearToEmpty();
}
inline const std::string& TestMessage::name() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.TestMessage.name)
  return _internal_name();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void TestMessage::set_name(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.TestMessage.name)
}
inline std::string* TestMessage::mutable_name() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_name();
  // @@protoc_insertion_point(field_mutable:test.TestMessage.name)
  return _s;
}
inline const std::string& TestMessage::_internal_name() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.name_.Get();
}
inline void TestMessage::_internal_set_name(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.Set(value, GetArena());
}
inline std::string* TestMessage::_internal_mutable_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.name_.Mutable( GetArena());
}
inline std::string* TestMessage::release_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.TestMessage.name)
  return _impl_.name_.Release();
}
inline void TestMessage::set_allocated_name(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.SetAllocated(value, GetArena());
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString() && _impl_.name_.IsDefault()) {
    _impl_.name_.Set("", GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:test.TestMessage.name)
}

// double value = 3;
inline void TestMessage::clear_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_ = 0;
}
inline double TestMessage::value() const {
  // @@protoc_insertion_point(field_get:test.TestMessage.value)
  return _internal_value();
}
inline void TestMessage::set_value(double value) {
  _internal_set_value(value);
  // @@protoc_insertion_point(field_set:test.TestMessage.value)
}
inline double TestMessage::_internal_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.value_;
}
inline void TestMessage::_internal_set_value(double value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_ = value;
}

// repeated string tags = 4;
inline int TestMessage::_internal_tags_size() const {
  return _internal_tags().size();
}
inline int TestMessage::tags_size() const {
  return _internal_tags_size();
}
inline void TestMessage::clear_tags() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.tags_.Clear();
}
inline std::string* TestMessage::add_tags() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  std::string* _s = _internal_mutable_tags()->Add();
  // @@protoc_insertion_point(field_add_mutable:test.TestMessage.tags)
  return _s;
}
inline const std::string& TestMessage::tags(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.TestMessage.tags)
  return _internal_tags().Get(index);
}
inline std::string* TestMessage::mutable_tags(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:test.TestMessage.tags)
  return _internal_mutable_tags()->Mutable(index);
}
template <typename Arg_, typename... Args_>
inline void TestMessage::set_tags(int index, Arg_&& value, Args_... args) {
  ::google::protobuf::internal::AssignToString(
      *_internal_mutable_tags()->Mutable(index),
      std::forward<Arg_>(value), args... );
  // @@protoc_insertion_point(field_set:test.TestMessage.tags)
}
template <typename Arg_, typename... Args_>
inline void TestMessage::add_tags(Arg_&& value, Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::google::protobuf::internal::AddToRepeatedPtrField(*_internal_mutable_tags(),
                               std::forward<Arg_>(value),
                               args... );
  // @@protoc_insertion_point(field_add:test.TestMessage.tags)
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
TestMessage::tags() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.TestMessage.tags)
  return _internal_tags();
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
TestMessage::mutable_tags() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.TestMessage.tags)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_tags();
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
TestMessage::_internal_tags() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.tags_;
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
TestMessage::_internal_mutable_tags() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.tags_;
}

// bool is_valid = 5;
inline void TestMessage::clear_is_valid() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.is_valid_ = false;
}
inline bool TestMessage::is_valid() const {
  // @@protoc_insertion_point(field_get:test.TestMessage.is_valid)
  return _internal_is_valid();
}
inline void TestMessage::set_is_valid(bool value) {
  _internal_set_is_valid(value);
  // @@protoc_insertion_point(field_set:test.TestMessage.is_valid)
}
inline bool TestMessage::_internal_is_valid() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.is_valid_;
}
inline void TestMessage::_internal_set_is_valid(bool value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.is_valid_ = value;
}

// string data = 6;
inline void TestMessage::clear_data() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.data_.ClearToEmpty();
}
inline const std::string& TestMessage::data() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.TestMessage.data)
  return _internal_data();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void TestMessage::set_data(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.data_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.TestMessage.data)
}
inline std::string* TestMessage::mutable_data() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_data();
  // @@protoc_insertion_point(field_mutable:test.TestMessage.data)
  return _s;
}
inline const std::string& TestMessage::_internal_data() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.data_.Get();
}
inline void TestMessage::_internal_set_data(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.data_.Set(value, GetArena());
}
inline std::string* TestMessage::_internal_mutable_data() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.data_.Mutable( GetArena());
}
inline std::string* TestMessage::release_data() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.TestMessage.data)
  return _impl_.data_.Release();
}
inline void TestMessage::set_allocated_data(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.data_.SetAllocated(value, GetArena());
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString() && _impl_.data_.IsDefault()) {
    _impl_.data_.Set("", GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:test.TestMessage.data)
}

// -------------------------------------------------------------------

// AnotherTestMessage

// int64 timestamp = 1;
inline void AnotherTestMessage::clear_timestamp() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.timestamp_ = ::int64_t{0};
}
inline ::int64_t AnotherTestMessage::timestamp() const {
  // @@protoc_insertion_point(field_get:test.AnotherTestMessage.timestamp)
  return _internal_timestamp();
}
inline void AnotherTestMessage::set_timestamp(::int64_t value) {
  _internal_set_timestamp(value);
  // @@protoc_insertion_point(field_set:test.AnotherTestMessage.timestamp)
}
inline ::int64_t AnotherTestMessage::_internal_timestamp() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.timestamp_;
}
inline void AnotherTestMessage::_internal_set_timestamp(::int64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.timestamp_ = value;
}

// string description = 2;
inline void AnotherTestMessage::clear_description() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.description_.ClearToEmpty();
}
inline const std::string& AnotherTestMessage::description() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.AnotherTestMessage.description)
  return _internal_description();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void AnotherTestMessage::set_description(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.description_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.AnotherTestMessage.description)
}
inline std::string* AnotherTestMessage::mutable_description() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_description();
  // @@protoc_insertion_point(field_mutable:test.AnotherTestMessage.description)
  return _s;
}
inline const std::string& AnotherTestMessage::_internal_description() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.description_.Get();
}
inline void AnotherTestMessage::_internal_set_description(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.description_.Set(value, GetArena());
}
inline std::string* AnotherTestMessage::_internal_mutable_description() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.description_.Mutable( GetArena());
}
inline std::string* AnotherTestMessage::release_description() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.AnotherTestMessage.description)
  return _impl_.description_.Release();
}
inline void AnotherTestMessage::set_allocated_description(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.description_.SetAllocated(value, GetArena());
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString() && _impl_.description_.IsDefault()) {
    _impl_.description_.Set("", GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:test.AnotherTestMessage.description)
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)
}  // namespace test


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // test_5fmessage_2eproto_2epb_2eh
