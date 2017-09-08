//===- CoverageMapping.h - Code coverage mapping support --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Code coverage mapping data is generated by clang and read by
// llvm-cov to show code coverage statistics for a file.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_PROFILEDATA_COVERAGE_COVERAGEMAPPING_H
#define LLVM_PROFILEDATA_COVERAGE_COVERAGEMAPPING_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/iterator.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/ProfileData/InstrProf.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace llvm {

class IndexedInstrProfReader;

namespace coverage {

class CoverageMappingReader;
struct CoverageMappingRecord;

enum class coveragemap_error {
  success = 0,
  eof,
  no_data_found,
  unsupported_version,
  truncated,
  malformed
};

const std::error_category &coveragemap_category();

inline std::error_code make_error_code(coveragemap_error E) {
  return std::error_code(static_cast<int>(E), coveragemap_category());
}

class CoverageMapError : public ErrorInfo<CoverageMapError> {
public:
  CoverageMapError(coveragemap_error Err) : Err(Err) {
    assert(Err != coveragemap_error::success && "Not an error");
  }

  std::string message() const override;

  void log(raw_ostream &OS) const override { OS << message(); }

  std::error_code convertToErrorCode() const override {
    return make_error_code(Err);
  }

  coveragemap_error get() const { return Err; }

  static char ID;

private:
  coveragemap_error Err;
};

/// A Counter is an abstract value that describes how to compute the
/// execution count for a region of code using the collected profile count data.
struct Counter {
  enum CounterKind { Zero, CounterValueReference, Expression };
  static const unsigned EncodingTagBits = 2;
  static const unsigned EncodingTagMask = 0x3;
  static const unsigned EncodingCounterTagAndExpansionRegionTagBits =
      EncodingTagBits + 1;

private:
  CounterKind Kind = Zero;
  unsigned ID = 0;

  Counter(CounterKind Kind, unsigned ID) : Kind(Kind), ID(ID) {}

public:
  Counter() = default;

  CounterKind getKind() const { return Kind; }

  bool isZero() const { return Kind == Zero; }

  bool isExpression() const { return Kind == Expression; }

  unsigned getCounterID() const { return ID; }

  unsigned getExpressionID() const { return ID; }

  friend bool operator==(const Counter &LHS, const Counter &RHS) {
    return LHS.Kind == RHS.Kind && LHS.ID == RHS.ID;
  }

  friend bool operator!=(const Counter &LHS, const Counter &RHS) {
    return !(LHS == RHS);
  }

  friend bool operator<(const Counter &LHS, const Counter &RHS) {
    return std::tie(LHS.Kind, LHS.ID) < std::tie(RHS.Kind, RHS.ID);
  }

  /// Return the counter that represents the number zero.
  static Counter getZero() { return Counter(); }

  /// Return the counter that corresponds to a specific profile counter.
  static Counter getCounter(unsigned CounterId) {
    return Counter(CounterValueReference, CounterId);
  }

  /// Return the counter that corresponds to a specific addition counter
  /// expression.
  static Counter getExpression(unsigned ExpressionId) {
    return Counter(Expression, ExpressionId);
  }
};

/// A Counter expression is a value that represents an arithmetic operation
/// with two counters.
struct CounterExpression {
  enum ExprKind { Subtract, Add };
  ExprKind Kind;
  Counter LHS, RHS;

  CounterExpression(ExprKind Kind, Counter LHS, Counter RHS)
      : Kind(Kind), LHS(LHS), RHS(RHS) {}
};

/// A Counter expression builder is used to construct the counter expressions.
/// It avoids unnecessary duplication and simplifies algebraic expressions.
class CounterExpressionBuilder {
  /// A list of all the counter expressions
  std::vector<CounterExpression> Expressions;

  /// A lookup table for the index of a given expression.
  DenseMap<CounterExpression, unsigned> ExpressionIndices;

  /// Return the counter which corresponds to the given expression.
  ///
  /// If the given expression is already stored in the builder, a counter
  /// that references that expression is returned. Otherwise, the given
  /// expression is added to the builder's collection of expressions.
  Counter get(const CounterExpression &E);

  /// Represents a term in a counter expression tree.
  struct Term {
    unsigned CounterID;
    int Factor;

    Term(unsigned CounterID, int Factor)
        : CounterID(CounterID), Factor(Factor) {}
  };

  /// Gather the terms of the expression tree for processing.
  ///
  /// This collects each addition and subtraction referenced by the counter into
  /// a sequence that can be sorted and combined to build a simplified counter
  /// expression.
  void extractTerms(Counter C, int Sign, SmallVectorImpl<Term> &Terms);

  /// Simplifies the given expression tree
  /// by getting rid of algebraically redundant operations.
  Counter simplify(Counter ExpressionTree);

public:
  ArrayRef<CounterExpression> getExpressions() const { return Expressions; }

  /// Return a counter that represents the expression that adds LHS and RHS.
  Counter add(Counter LHS, Counter RHS);

  /// Return a counter that represents the expression that subtracts RHS from
  /// LHS.
  Counter subtract(Counter LHS, Counter RHS);
};

using LineColPair = std::pair<unsigned, unsigned>;

/// A Counter mapping region associates a source range with a specific counter.
struct CounterMappingRegion {
  enum RegionKind {
    /// A CodeRegion associates some code with a counter
    CodeRegion,

    /// An ExpansionRegion represents a file expansion region that associates 
    /// a source range with the expansion of a virtual source file, such as
    /// for a macro instantiation or #include file.
    ExpansionRegion,

    /// A SkippedRegion represents a source range with code that was skipped
    /// by a preprocessor or similar means.
    SkippedRegion
  };

  Counter Count;
  unsigned FileID, ExpandedFileID;
  unsigned LineStart, ColumnStart, LineEnd, ColumnEnd;
  RegionKind Kind;

  CounterMappingRegion(Counter Count, unsigned FileID, unsigned ExpandedFileID,
                       unsigned LineStart, unsigned ColumnStart,
                       unsigned LineEnd, unsigned ColumnEnd, RegionKind Kind)
      : Count(Count), FileID(FileID), ExpandedFileID(ExpandedFileID),
        LineStart(LineStart), ColumnStart(ColumnStart), LineEnd(LineEnd),
        ColumnEnd(ColumnEnd), Kind(Kind) {}

  static CounterMappingRegion
  makeRegion(Counter Count, unsigned FileID, unsigned LineStart,
             unsigned ColumnStart, unsigned LineEnd, unsigned ColumnEnd) {
    return CounterMappingRegion(Count, FileID, 0, LineStart, ColumnStart,
                                LineEnd, ColumnEnd, CodeRegion);
  }

  static CounterMappingRegion
  makeExpansion(unsigned FileID, unsigned ExpandedFileID, unsigned LineStart,
                unsigned ColumnStart, unsigned LineEnd, unsigned ColumnEnd) {
    return CounterMappingRegion(Counter(), FileID, ExpandedFileID, LineStart,
                                ColumnStart, LineEnd, ColumnEnd,
                                ExpansionRegion);
  }

  static CounterMappingRegion
  makeSkipped(unsigned FileID, unsigned LineStart, unsigned ColumnStart,
              unsigned LineEnd, unsigned ColumnEnd) {
    return CounterMappingRegion(Counter(), FileID, 0, LineStart, ColumnStart,
                                LineEnd, ColumnEnd, SkippedRegion);
  }

  inline LineColPair startLoc() const {
    return LineColPair(LineStart, ColumnStart);
  }

  inline LineColPair endLoc() const { return LineColPair(LineEnd, ColumnEnd); }
};

/// Associates a source range with an execution count.
struct CountedRegion : public CounterMappingRegion {
  uint64_t ExecutionCount;

  CountedRegion(const CounterMappingRegion &R, uint64_t ExecutionCount)
      : CounterMappingRegion(R), ExecutionCount(ExecutionCount) {}
};

/// A Counter mapping context is used to connect the counters, expressions
/// and the obtained counter values.
class CounterMappingContext {
  ArrayRef<CounterExpression> Expressions;
  ArrayRef<uint64_t> CounterValues;

public:
  CounterMappingContext(ArrayRef<CounterExpression> Expressions,
                        ArrayRef<uint64_t> CounterValues = None)
      : Expressions(Expressions), CounterValues(CounterValues) {}

  void setCounts(ArrayRef<uint64_t> Counts) { CounterValues = Counts; }

  void dump(const Counter &C, raw_ostream &OS) const;
  void dump(const Counter &C) const { dump(C, dbgs()); }

  /// Return the number of times that a region of code associated with this
  /// counter was executed.
  Expected<int64_t> evaluate(const Counter &C) const;
};

/// Code coverage information for a single function.
struct FunctionRecord {
  /// Raw function name.
  std::string Name;
  /// Associated files.
  std::vector<std::string> Filenames;
  /// Regions in the function along with their counts.
  std::vector<CountedRegion> CountedRegions;
  /// The number of times this function was executed.
  uint64_t ExecutionCount;

  FunctionRecord(StringRef Name, ArrayRef<StringRef> Filenames)
      : Name(Name), Filenames(Filenames.begin(), Filenames.end()) {}

  FunctionRecord(FunctionRecord &&FR) = default;
  FunctionRecord &operator=(FunctionRecord &&) = default;

  void pushRegion(CounterMappingRegion Region, uint64_t Count) {
    if (CountedRegions.empty())
      ExecutionCount = Count;
    CountedRegions.emplace_back(Region, Count);
  }
};

/// Iterator over Functions, optionally filtered to a single file.
class FunctionRecordIterator
    : public iterator_facade_base<FunctionRecordIterator,
                                  std::forward_iterator_tag, FunctionRecord> {
  ArrayRef<FunctionRecord> Records;
  ArrayRef<FunctionRecord>::iterator Current;
  StringRef Filename;

  /// Skip records whose primary file is not \c Filename.
  void skipOtherFiles();

public:
  FunctionRecordIterator(ArrayRef<FunctionRecord> Records_,
                         StringRef Filename = "")
      : Records(Records_), Current(Records.begin()), Filename(Filename) {
    skipOtherFiles();
  }

  FunctionRecordIterator() : Current(Records.begin()) {}

  bool operator==(const FunctionRecordIterator &RHS) const {
    return Current == RHS.Current && Filename == RHS.Filename;
  }

  const FunctionRecord &operator*() const { return *Current; }

  FunctionRecordIterator &operator++() {
    assert(Current != Records.end() && "incremented past end");
    ++Current;
    skipOtherFiles();
    return *this;
  }
};

/// Coverage information for a macro expansion or #included file.
///
/// When covered code has pieces that can be expanded for more detail, such as a
/// preprocessor macro use and its definition, these are represented as
/// expansions whose coverage can be looked up independently.
struct ExpansionRecord {
  /// The abstract file this expansion covers.
  unsigned FileID;
  /// The region that expands to this record.
  const CountedRegion &Region;
  /// Coverage for the expansion.
  const FunctionRecord &Function;

  ExpansionRecord(const CountedRegion &Region,
                  const FunctionRecord &Function)
      : FileID(Region.ExpandedFileID), Region(Region), Function(Function) {}
};

/// The execution count information starting at a point in a file.
///
/// A sequence of CoverageSegments gives execution counts for a file in format
/// that's simple to iterate through for processing.
struct CoverageSegment {
  /// The line where this segment begins.
  unsigned Line;
  /// The column where this segment begins.
  unsigned Col;
  /// The execution count, or zero if no count was recorded.
  uint64_t Count;
  /// When false, the segment was uninstrumented or skipped.
  bool HasCount;
  /// Whether this enters a new region or returns to a previous count.
  bool IsRegionEntry;

  CoverageSegment(unsigned Line, unsigned Col, bool IsRegionEntry)
      : Line(Line), Col(Col), Count(0), HasCount(false),
        IsRegionEntry(IsRegionEntry) {}

  CoverageSegment(unsigned Line, unsigned Col, uint64_t Count,
                  bool IsRegionEntry)
      : Line(Line), Col(Col), Count(Count), HasCount(true),
        IsRegionEntry(IsRegionEntry) {}

  friend bool operator==(const CoverageSegment &L, const CoverageSegment &R) {
    return std::tie(L.Line, L.Col, L.Count, L.HasCount, L.IsRegionEntry) ==
           std::tie(R.Line, R.Col, R.Count, R.HasCount, R.IsRegionEntry);
  }
};

/// An instantiation group contains a \c FunctionRecord list, such that each
/// record corresponds to a distinct instantiation of the same function.
///
/// Note that it's possible for a function to have more than one instantiation
/// (consider C++ template specializations or static inline functions).
class InstantiationGroup {
  friend class CoverageMapping;

  unsigned Line;
  unsigned Col;
  std::vector<const FunctionRecord *> Instantiations;

  InstantiationGroup(unsigned Line, unsigned Col,
                     std::vector<const FunctionRecord *> Instantiations)
      : Line(Line), Col(Col), Instantiations(std::move(Instantiations)) {}

public:
  InstantiationGroup(const InstantiationGroup &) = delete;
  InstantiationGroup(InstantiationGroup &&) = default;

  /// Get the number of instantiations in this group.
  size_t size() const { return Instantiations.size(); }

  /// Get the line where the common function was defined.
  unsigned getLine() const { return Line; }

  /// Get the column where the common function was defined.
  unsigned getColumn() const { return Col; }

  /// Check if the instantiations in this group have a common mangled name.
  bool hasName() const {
    for (unsigned I = 1, E = Instantiations.size(); I < E; ++I)
      if (Instantiations[I]->Name != Instantiations[0]->Name)
        return false;
    return true;
  }

  /// Get the common mangled name for instantiations in this group.
  StringRef getName() const {
    assert(hasName() && "Instantiations don't have a shared name");
    return Instantiations[0]->Name;
  }

  /// Get the total execution count of all instantiations in this group.
  uint64_t getTotalExecutionCount() const {
    uint64_t Count = 0;
    for (const FunctionRecord *F : Instantiations)
      Count += F->ExecutionCount;
    return Count;
  }

  /// Get the instantiations in this group.
  ArrayRef<const FunctionRecord *> getInstantiations() const {
    return Instantiations;
  }
};

/// Coverage information to be processed or displayed.
///
/// This represents the coverage of an entire file, expansion, or function. It
/// provides a sequence of CoverageSegments to iterate through, as well as the
/// list of expansions that can be further processed.
class CoverageData {
  friend class CoverageMapping;

  std::string Filename;
  std::vector<CoverageSegment> Segments;
  std::vector<ExpansionRecord> Expansions;

public:
  CoverageData() = default;

  CoverageData(StringRef Filename) : Filename(Filename) {}

  /// Get the name of the file this data covers.
  StringRef getFilename() const { return Filename; }

  std::vector<CoverageSegment>::const_iterator begin() const {
    return Segments.begin();
  }

  std::vector<CoverageSegment>::const_iterator end() const {
    return Segments.end();
  }

  bool empty() const { return Segments.empty(); }

  /// Expansions that can be further processed.
  ArrayRef<ExpansionRecord> getExpansions() const { return Expansions; }
};

/// The mapping of profile information to coverage data.
///
/// This is the main interface to get coverage information, using a profile to
/// fill out execution counts.
class CoverageMapping {
  StringSet<> FunctionNames;
  std::vector<FunctionRecord> Functions;
  unsigned MismatchedFunctionCount = 0;

  CoverageMapping() = default;

  /// Add a function record corresponding to \p Record.
  Error loadFunctionRecord(const CoverageMappingRecord &Record,
                           IndexedInstrProfReader &ProfileReader);

public:
  CoverageMapping(const CoverageMapping &) = delete;
  CoverageMapping &operator=(const CoverageMapping &) = delete;

  /// Load the coverage mapping using the given readers.
  static Expected<std::unique_ptr<CoverageMapping>>
  load(ArrayRef<std::unique_ptr<CoverageMappingReader>> CoverageReaders,
       IndexedInstrProfReader &ProfileReader);

  /// Load the coverage mapping from the given object files and profile. If
  /// \p Arches is non-empty, it must specify an architecture for each object.
  static Expected<std::unique_ptr<CoverageMapping>>
  load(ArrayRef<StringRef> ObjectFilenames, StringRef ProfileFilename,
       ArrayRef<StringRef> Arches = None);

  /// The number of functions that couldn't have their profiles mapped.
  ///
  /// This is a count of functions whose profile is out of date or otherwise
  /// can't be associated with any coverage information.
  unsigned getMismatchedCount() { return MismatchedFunctionCount; }

  /// Returns a lexicographically sorted, unique list of files that are
  /// covered.
  std::vector<StringRef> getUniqueSourceFiles() const;

  /// Get the coverage for a particular file.
  ///
  /// The given filename must be the name as recorded in the coverage
  /// information. That is, only names returned from getUniqueSourceFiles will
  /// yield a result.
  CoverageData getCoverageForFile(StringRef Filename) const;

  /// Get the coverage for a particular function.
  CoverageData getCoverageForFunction(const FunctionRecord &Function) const;

  /// Get the coverage for an expansion within a coverage set.
  CoverageData getCoverageForExpansion(const ExpansionRecord &Expansion) const;

  /// Gets all of the functions covered by this profile.
  iterator_range<FunctionRecordIterator> getCoveredFunctions() const {
    return make_range(FunctionRecordIterator(Functions),
                      FunctionRecordIterator());
  }

  /// Gets all of the functions in a particular file.
  iterator_range<FunctionRecordIterator>
  getCoveredFunctions(StringRef Filename) const {
    return make_range(FunctionRecordIterator(Functions, Filename),
                      FunctionRecordIterator());
  }

  /// Get the list of function instantiation groups in a particular file.
  ///
  /// Every instantiation group in a program is attributed to exactly one file:
  /// the file in which the definition for the common function begins.
  std::vector<InstantiationGroup>
  getInstantiationGroups(StringRef Filename) const;
};

// Profile coverage map has the following layout:
// [CoverageMapFileHeader]
// [ArrayStart]
//  [CovMapFunctionRecord]
//  [CovMapFunctionRecord]
//  ...
// [ArrayEnd]
// [Encoded Region Mapping Data]
LLVM_PACKED_START
template <class IntPtrT> struct CovMapFunctionRecordV1 {
#define COVMAP_V1
#define COVMAP_FUNC_RECORD(Type, LLVMType, Name, Init) Type Name;
#include "llvm/ProfileData/InstrProfData.inc"
#undef COVMAP_V1

  // Return the structural hash associated with the function.
  template <support::endianness Endian> uint64_t getFuncHash() const {
    return support::endian::byte_swap<uint64_t, Endian>(FuncHash);
  }

  // Return the coverage map data size for the funciton.
  template <support::endianness Endian> uint32_t getDataSize() const {
    return support::endian::byte_swap<uint32_t, Endian>(DataSize);
  }

  // Return function lookup key. The value is consider opaque.
  template <support::endianness Endian> IntPtrT getFuncNameRef() const {
    return support::endian::byte_swap<IntPtrT, Endian>(NamePtr);
  }

  // Return the PGO name of the function */
  template <support::endianness Endian>
  Error getFuncName(InstrProfSymtab &ProfileNames, StringRef &FuncName) const {
    IntPtrT NameRef = getFuncNameRef<Endian>();
    uint32_t NameS = support::endian::byte_swap<uint32_t, Endian>(NameSize);
    FuncName = ProfileNames.getFuncName(NameRef, NameS);
    if (NameS && FuncName.empty())
      return make_error<CoverageMapError>(coveragemap_error::malformed);
    return Error::success();
  }
};

struct CovMapFunctionRecord {
#define COVMAP_FUNC_RECORD(Type, LLVMType, Name, Init) Type Name;
#include "llvm/ProfileData/InstrProfData.inc"

  // Return the structural hash associated with the function.
  template <support::endianness Endian> uint64_t getFuncHash() const {
    return support::endian::byte_swap<uint64_t, Endian>(FuncHash);
  }

  // Return the coverage map data size for the funciton.
  template <support::endianness Endian> uint32_t getDataSize() const {
    return support::endian::byte_swap<uint32_t, Endian>(DataSize);
  }

  // Return function lookup key. The value is consider opaque.
  template <support::endianness Endian> uint64_t getFuncNameRef() const {
    return support::endian::byte_swap<uint64_t, Endian>(NameRef);
  }

  // Return the PGO name of the function */
  template <support::endianness Endian>
  Error getFuncName(InstrProfSymtab &ProfileNames, StringRef &FuncName) const {
    uint64_t NameRef = getFuncNameRef<Endian>();
    FuncName = ProfileNames.getFuncName(NameRef);
    return Error::success();
  }
};

// Per module coverage mapping data header, i.e. CoverageMapFileHeader
// documented above.
struct CovMapHeader {
#define COVMAP_HEADER(Type, LLVMType, Name, Init) Type Name;
#include "llvm/ProfileData/InstrProfData.inc"
  template <support::endianness Endian> uint32_t getNRecords() const {
    return support::endian::byte_swap<uint32_t, Endian>(NRecords);
  }

  template <support::endianness Endian> uint32_t getFilenamesSize() const {
    return support::endian::byte_swap<uint32_t, Endian>(FilenamesSize);
  }

  template <support::endianness Endian> uint32_t getCoverageSize() const {
    return support::endian::byte_swap<uint32_t, Endian>(CoverageSize);
  }

  template <support::endianness Endian> uint32_t getVersion() const {
    return support::endian::byte_swap<uint32_t, Endian>(Version);
  }
};

LLVM_PACKED_END

enum CovMapVersion {
  Version1 = 0,
  // Function's name reference from CovMapFuncRecord is changed from raw
  // name string pointer to MD5 to support name section compression. Name
  // section is also compressed.
  Version2 = 1,
  // The current version is Version2
  CurrentVersion = INSTR_PROF_COVMAP_VERSION
};

template <int CovMapVersion, class IntPtrT> struct CovMapTraits {
  using CovMapFuncRecordType = CovMapFunctionRecord;
  using NameRefType = uint64_t;
};

template <class IntPtrT> struct CovMapTraits<CovMapVersion::Version1, IntPtrT> {
  using CovMapFuncRecordType = CovMapFunctionRecordV1<IntPtrT>;
  using NameRefType = IntPtrT;
};

} // end namespace coverage

/// Provide DenseMapInfo for CounterExpression
template<> struct DenseMapInfo<coverage::CounterExpression> {
  static inline coverage::CounterExpression getEmptyKey() {
    using namespace coverage;

    return CounterExpression(CounterExpression::ExprKind::Subtract,
                             Counter::getCounter(~0U),
                             Counter::getCounter(~0U));
  }

  static inline coverage::CounterExpression getTombstoneKey() {
    using namespace coverage;

    return CounterExpression(CounterExpression::ExprKind::Add,
                             Counter::getCounter(~0U),
                             Counter::getCounter(~0U));
  }

  static unsigned getHashValue(const coverage::CounterExpression &V) {
    return static_cast<unsigned>(
        hash_combine(V.Kind, V.LHS.getKind(), V.LHS.getCounterID(),
                     V.RHS.getKind(), V.RHS.getCounterID()));
  }

  static bool isEqual(const coverage::CounterExpression &LHS,
                      const coverage::CounterExpression &RHS) {
    return LHS.Kind == RHS.Kind && LHS.LHS == RHS.LHS && LHS.RHS == RHS.RHS;
  }
};

} // end namespace llvm

#endif // LLVM_PROFILEDATA_COVERAGE_COVERAGEMAPPING_H
