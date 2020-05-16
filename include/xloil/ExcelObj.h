#pragma once
#include <xlOil/XlCallSlim.h>
#include <xlOil/ExportMacro.h>
#include <xlOil/PString.h>
#include <string>
#include <array>
#include <cassert>
#include <optional>

#define XLOIL_XLOPER msxll::xloper12

namespace xloil
{
  constexpr uint16_t XL_CELL_ADDRESS_A1_MAX_LEN = 3 + 7 + 1 + 3 + 7 + 1;
  constexpr uint16_t XL_CELL_ADDRESS_RC_MAX_LEN = 29;
  constexpr uint16_t XL_SHEET_NAME_MAX_LEN = 31;
  constexpr uint16_t XL_STRING_MAX_LEN = 32767;

  /// <summary>
  /// Describes the underlying type of the variant <see cref="ExcelObj"/>
  /// </summary>
  enum class ExcelType
  {
    Num = 0x0001,
    Str = 0x0002,
    Bool = 0x0004,
    Ref = 0x0008,
    Err = 0x0010,
    Flow = 0x0020,
    Multi = 0x0040,
    Missing = 0x0080,
    Nil = 0x0100,
    SRef = 0x0400,
    Int = 0x0800,
    BigData = msxll::xltypeStr | msxll::xltypeInt,

    // Types that can be elements of an array
    ArrayValue = Num | Str | Bool | Err | Int | Nil,

    // Types which refer to ranges
    RangeRef = SRef | Ref,

    // Types which do not have external memory allocation
    Simple = Num | Bool | SRef | Missing | Nil | Int | Err
  };

  /// <summary>
  /// Describes the various error types Excel can handle and display
  /// </summary>
  enum class CellError
  {
    Null = msxll::xlerrNull,
    Div0 = msxll::xlerrDiv0,
    Value = msxll::xlerrValue,
    Ref = msxll::xlerrRef,
    Name = msxll::xlerrName,
    Num = msxll::xlerrNum,
    NA = msxll::xlerrNA,
    GettingData = msxll::xlerrGettingData
  };

  /// <summary>
  /// Array of all CellError types, useful for autogeneration
  /// of bindings
  /// </summary>
  static const std::array<CellError, 8> theCellErrors =
  {
    CellError::Null,
    CellError::Div0,
    CellError::Value,
    CellError::Ref,
    CellError::Name,
    CellError::Num,
    CellError::NA,
    CellError::GettingData
  };

  XLOIL_EXPORT const wchar_t* enumAsWCString(CellError e);
  XLOIL_EXPORT const wchar_t* enumAsWCString(ExcelType e);

  class ExcelArray;

  class ExcelObj;
  namespace Const
  {
    /// <summary>
    /// A static ExcelObj set to Missing type.
    /// </summary>
    XLOIL_EXPORT const ExcelObj& Missing();
    /// <summary>
    /// A static ExcelObj set to the specified error type
    /// </summary>
    XLOIL_EXPORT const ExcelObj& Error(CellError e);
    /// <summary>
    /// A static ExcelObj containing an empty string
    /// </summary>
    XLOIL_EXPORT const ExcelObj& EmptyStr();
  }

  /// <summary>
  /// Wraps an XLL variant-type (xloper12) providing many useful and safe
  /// methods to create, copy and modify the data.
  /// 
  /// An ExcelObj can be statically cast to an xloper12 and vice-versa.
  /// In fact, the underlying xloper12 members can be manipulated directly,
  /// although this is not recommended.
  /// </summary>
  class XLOIL_EXPORT ExcelObj : public XLOIL_XLOPER
  {
  public:
    typedef wchar_t Char;
    typedef XLOIL_XLOPER Base;
    using row_t = uint32_t;
    using col_t = uint32_t;

    ExcelObj()
    {
      xltype = msxll::xltypeNil;
    }

    /// <summary>
    /// Constructor for integral types
    /// </summary>
    template <class T,
      std::enable_if_t<std::is_integral<T>::value, int> = 0>
    explicit ExcelObj(T x)
    {
      xltype = msxll::xltypeInt;
      val.w = (int)x;
    }

    /// <summary>
    /// Constructor for floating point types
    /// </summary>
    template <class T,
      std::enable_if_t<std::is_floating_point<T>::value, int> = 0>
    explicit ExcelObj(T d)
    {
      if (std::isnan(d))
      {
        val.err = msxll::xlerrNum;
        xltype = msxll::xltypeErr;
      }
      else
      {
        xltype = msxll::xltypeNum;
        val.num = (double)d;
      }
    }

    /// <summary>
    /// Construct from a bool
    /// </summary>
    explicit ExcelObj(bool b)
    {
      xltype = msxll::xltypeBool;
      val.xbool = b ? 1 : 0;
    }

    /// <summary>
    /// Creates an empty object of the specified type. "Empty" in this case
    /// means a sensible default depending on the data type.  For bool's it 
    /// is false, for numerics zero, for string it's the empty string, 
    /// for the error type it is \#N/A.
    /// </summary>
    /// <param name=""></param>
    ExcelObj(ExcelType);

    /// <summary>
    /// Construct from char string
    /// </summary>
    explicit ExcelObj(const char*);

    /// <summary>
    /// Construct from wide-char string
    /// </summary>
    explicit ExcelObj(const wchar_t*);

    /// <summary>
    /// Construct from STL string
    /// </summary>
    template <class T>
    explicit ExcelObj(const std::basic_string<T>& s)
      : ExcelObj(s.c_str())
    {}

    /// Move ctor from owned Pascal string buffer.
    explicit ExcelObj(PString<Char>&& pstr)
    {
      val.str = pstr.release();
      if (!val.str)
        val.str = Const::EmptyStr().val.str;
      xltype = msxll::xltypeStr;
    }

    ExcelObj(nullptr_t)
    {
      xltype = msxll::xltypeMissing;
    }

    ExcelObj(CellError err)
    {
      val.err = (int)err;
      xltype = msxll::xltypeErr;
    }

    /// <summary>
    /// Constructs an array from data. Takes ownership of data, which
    /// must be correctly arranged in memory. Use with caution!
    /// </summary>
    ExcelObj(const ExcelObj* data, int nRows, int nCols)
    {
      val.array.rows = nRows;
      val.array.columns = nCols;
      val.array.lparray = (Base*)data;
      xltype = msxll::xltypeMulti;
    }

    /// <summary>
    /// Catch constructor: purpose is to avoid pointers
    /// being auto-cast to integral / bool types
    /// </summary>
    template <class T> explicit ExcelObj(T* t) { static_assert(false); }

    /// <summary>
    /// Copy constructor
    /// </summary>
    ExcelObj(const ExcelObj& that)
    {
      overwrite(*this, that);
    }

    /// <summary>
    /// Move constructor
    /// </summary>
    ExcelObj(ExcelObj&& donor)
    {
      (Base&)*this = donor;
      // Mark donor object as empty
      donor.xltype = msxll::xltypeNil;
    }

    ExcelObj::~ExcelObj()
    {
      reset();
    }

    /// <summary>
    /// Assignment from ExcelObj
    /// </summary>
    ExcelObj& operator=(const ExcelObj& that)
    {
      if (this == &that)
        return *this;
      copy(*this, that);
      return *this;
    }

    /// <summary>
    /// Move assignment
    /// </summary>
    template <class TDonor>
    ExcelObj& operator=(TDonor&& that)
    {
      *this = ExcelObj(that);
    }

    ExcelObj& operator=(ExcelObj&& donor)
    {
      reset();
      (Base&)*this = donor;
      // Mark donor object as empty
      donor.xltype = msxll::xltypeNil;
      return *this;
    }

    /// <summary>
    /// Deletes object content and sets it to \#N/A
    /// </summary>
    void reset();

    bool operator==(const ExcelObj& that) const
    {
      return compare(*this, that) == 0;
    }
    bool operator<(const ExcelObj& that) const
    {
      return compare(*this, that) == -1;
    }
    bool operator<=(const ExcelObj& that) const
    {
      return compare(*this, that) != 1;
    }

    /// <summary>
    /// Returns -1 if left < right, 0 if left == right, else 1.
    /// 
    /// When the types of <paramref name="left"/> and <paramref name="right"/> are 
    /// the same, numeric and string types are compared in the expected way. Arrays
    /// are compared by size then by pointer (a fast default). Refs are compared as
    /// address strings.  BigData and Flow types cannot be compared and return zero.
    /// 
    /// When the types differ, numeric types can still be compared but all others 
    /// are sorted by type number as repeated string conversions such as in a sort
    /// would be very expensive.
    /// </summary>
    /// <param name="left"></param>
    /// <param name="right"></param>
    /// <param name="caseSensitive">Whether string comparison is case sensitive</param>
    /// <returns>-1, 0 or +1</returns>
    static int compare(
      const ExcelObj& left,
      const ExcelObj& right,
      bool caseSensitive = false) noexcept;

    const Base* xloper() const { return this; }
    const Base* xloper() { return this; }

    /// <summary>
    /// Returns true if this ExcelObj has Missing type.
    /// </summary>
    bool ExcelObj::isMissing() const
    {
      return (xtype() & msxll::xltypeMissing) != 0;
    }

    /// <summary>
    /// Returns true if value is *not* one of: Missing, Nil
    /// error \#N/A or an empty string.
    /// </summary>
    /// <returns></returns>
    bool ExcelObj::isNonEmpty() const
    {
      using namespace msxll;
      switch (xtype())
      {
      case xltypeErr:
        return val.err != xlerrNA;
      case xltypeMissing:
      case xltypeNil:
        return false;
      case xltypeStr:
        return val.str[0] != L'\0';
      default:
        return true;
      }
    }
    
    /// <summary>
    /// Returns true if this ExcelObj is a \#N/A error
    /// </summary>
    /// <returns></returns>
    bool isNA() const
    {
      return isType(ExcelType::Err) && val.err == msxll::xlerrNA;
    }

    /// <summary>
    /// Get an enum describing the data contained in the ExcelObj
    /// </summary>
    /// <returns></returns>
    ExcelType ExcelObj::type() const
    {
      return ExcelType(xtype());
    }

    /// <summary>
    /// Returns true if this ExcelObj is of the specified type. This also
    /// works for compound types like ArrayValue and RangeRef that can't 
    /// be checked for by equality with type().
    /// </summary>
    bool isType(ExcelType type) const
    {
      return (xltype & (int)type) != 0;
    }

    /// <summary>
    /// Converts to a string. Attempts to stringify the various excel types.
    /// 
    /// The function recurses row-wise over Arrays and ranges and 
    /// concatenates the result. An optional separator may be given to
    /// insert between array/range entries.
    /// </summary>
    /// <param name="separator">optional separator to use for arrays</param>
    /// <returns></returns>
    std::wstring toString(const wchar_t* separator = nullptr) const;

    /// <summary>
    /// Similar to toString but more suitable for output of object 
    /// descriptions, for example in error messages. For this reason
    /// it doesn't throw but rather returns ``\<ERROR\>`` on failure.
    /// 
    /// Returns the same as toString except for arrays which yield 
    /// '[NxM]' where N and M are the number of rows and columns and 
    /// for ranges which return the range reference in the form 'Sheet!A1'.
    /// </summary>
    /// <returns></returns>
    std::wstring toStringRepresentation() const noexcept;

    /// <summary>
    /// Gives the maximum string length if toString is called on
    /// this object without actually attempting the conversion.
    /// This method is very fast except for arrays.
    /// </summary>
    size_t maxStringLength() const noexcept;

    /// <summary>
    /// Returns the string length if this object is a string, else zero.
    /// </summary>
    size_t stringLength() const
    {
      return xltype == msxll::xltypeStr ? val.str[0] : 0;
    }

    /// <summary>
    /// Returns a double from the object value type with some conversions:
    /// 
    /// int -> double
    /// bool -> 0 or 1
    /// missing/empty -> provided default
    /// 
    /// Other types will throw an exception.
    /// </summary>
    double toDouble(const std::optional<double> default = std::optional<double>()) const;

    /// <summary>
    /// Returns a int from the object value type with some conversions:
    /// 
    /// double -> int (if it can be stored as an int)
    /// bool -> 0 or 1
    /// missing/empty -> provided default
    /// 
    /// Other types will throw an exception.
    /// </summary>
    int toInt(const std::optional<int> default = std::optional<int>()) const;

    /// <summary>
    /// Returns a bool from the object value type with some conversions:
    /// 
    /// double/int -> false if exactly zero, true otherwise
    /// missing/empty -> provided default
    /// 
    /// Other types will throw an exception.
    /// </summary>
    bool toBool(const std::optional<bool> default = std::optional<bool>()) const;

    /// <summary>
    /// Returns the value in the ExcelObj assuming it is a double.
    /// If it isn't, will return nonsense (UB)
    /// </summary>
    double ExcelObj::asDouble() const
    {
      assert(xtype() == msxll::xltypeNum);
      return val.num;
    }

    /// <summary>
    /// Returns the value in the ExcelObj assuming it is an int.
    /// If it isn't, will return nonsense (UB)
    /// </summary>
    int ExcelObj::asInt() const
    {
      assert(xtype() == msxll::xltypeInt);
      return val.w;
    }

    /// <summary>
    /// Returns the value in the ExcelObj assuming it is a bool.
    /// If it isn't, will return nonsense (UB)
    /// </summary>
    bool ExcelObj::asBool() const
    {
      assert(xtype() == msxll::xltypeBool);
      return val.xbool;
    }

    /// <summary>
    /// Returns the value in the ExcelObj assuming it is an array
    /// If it isn't, will return nonsense (UB)
    /// </summary>
    /// 
    const ExcelObj* ExcelObj::asArray() const
    {
      assert(xtype() == msxll::xltypeMulti);
      return (const ExcelObj*)val.array.lparray;
    }

    /// <summary>
    /// Returns a PStringView object of the object's string data.
    /// If the object is not of string type, the resulting view 
    /// will be empty.
    /// </summary>
    PStringView<> ExcelObj::asPascalStr() const
    {
      return PStringView<>((xltype & msxll::xltypeStr) == 0 ? nullptr : val.str);
    }

    /// <summary>
    /// Call this on function result objects received from Excel to 
    /// declare that Excel must free them. This is automatically done
    /// by callExcel/tryCallExcel so only invoke this if you use Excel12v
    /// directly.
    /// </summary>
    /// <returns></returns>
    ExcelObj & fromExcel()
    {
      xltype |= msxll::xlbitXLFree;
      return *this;
    }

    /// <summary>
    /// Returns a pointer to the current object suitable for returning to Excel
    /// as the result of a function. Modifies a flag to tell Excel that xlOil 
    /// will need a callback to free the memory. This method or 
    /// <see cref="ExcelObj::returnValue"/> must be used for final object passed 
    /// back to Excel. It should not be used anywhere else.
    /// </summary>
    /// <returns></returns>
    ExcelObj * toExcel()
    {
      xltype |= msxll::xlbitDLLFree;
      return this;
    }

    /// <summary>
    /// Constructs an ExcelObj from the given arguments, setting a flag to tell 
    /// Excel that xlOil will need a callback to free the memory. **This method must
    /// be used for final object passed back to Excel. It must not be used anywhere
    /// else**.
    /// </summary>
    template<class... Args>
    static ExcelObj* returnValue(Args&&... args)
    {
      return (new ExcelObj(std::forward<Args>(args)...))->toExcel();
    }
    static ExcelObj* returnValue(CellError err)
    {
      return const_cast<ExcelObj*>(&Const::Error(err));
    }
    static ExcelObj* returnValue(const std::exception& e)
    {
      return returnValue(e.what());
    }

    template<>
    static ExcelObj* returnValue(ExcelObj&& p)
    {
      // same as the args, but want to make this one explicit
      return (new ExcelObj(std::forward<ExcelObj>(p)))->toExcel();
    }

    /// <summary>
    /// Attempts to convert the ExcelObj to a Date. This will only
    /// succeed for numeric types.
    /// </summary>
    /// <param name="nDay"></param>
    /// <param name="nMonth"></param>
    /// <param name="nYear"></param>
    /// <returns>true if conversion suceeds, else false</returns>
    bool toDMY(int &nDay, int &nMonth, int &nYear) const noexcept;

    /// <summary>
    /// Attempts to convert the ExcelObj to a Date/Time.  This will only
    /// succeed for numeric types.
    /// </summary>
    /// <param name="nDay"></param>
    /// <param name="nMonth"></param>
    /// <param name="nYear"></param>
    /// <param name="nHours"></param>
    /// <param name="nMins"></param>
    /// <param name="nSecs"></param>
    /// <param name="uSecs"></param>
    /// <returns></returns>
    bool toDMYHMS(int &nDay, int &nMonth, int &nYear, int& nHours,
      int& nMins, int& nSecs, int& uSecs) const noexcept;

    /// <summary>
    /// Attempts to convert the ExcelObj to a std::tm struct. If the ExcelObj
    /// is a string and coerce is true, it will attempt to parse that string
    /// using the given string format or if none was provided, all registered 
    /// string formats. 
    /// 
    /// Only relevant values in the tm struct will be populated, so you may 
    /// want to zero-initialise the struct before calling this function.
    /// </summary>
    /// <param name="datetime">The tm struct to be populated.</param>
    /// <param name="coerce">If true, attempt to parse strings</param>
    /// <param name="format">The date format for string parsing</param>
    /// <returns>true if conversion suceeds, else false</returns>
    bool toDateTime(
      std::tm& datetime, 
      const bool coerce = false, 
      const wchar_t* format = nullptr) const;

    /// <summary>
    /// Called by ExcelArray to determine the size of array data when
    /// blanks and \#N/A is ignored.
    /// </summary>
    /// <param name="nRows"></param>
    /// <param name="nCols"></param>
    /// <returns>false if object is not an array, else true</returns>
    bool trimmedArraySize(row_t& nRows, col_t& nCols) const;

    /// <summary>
    /// Destroys the target object and replaces it with the source
    /// </summary>
    /// <param name="to"></param>
    /// <param name="from"></param>
    static void copy(ExcelObj& to, const ExcelObj& from)
    {
      to.reset();
      overwrite(to, from);
    }

    /// <summary>
    /// Replaces the target object with the source without calling 
    /// the target's destructor.  Dangerous, but useful for uninitialised
    /// or non-allocating (e.g. numeric, error, nil) target objects.
    /// </summary>
    /// <param name="to"></param>
    /// <param name="from"></param>
    static void overwrite(ExcelObj& to, const ExcelObj& from)
    {
      if (from.isType(ExcelType::Simple))
        (msxll::XLOPER12&)to = (const msxll::XLOPER12&)from;
      else
        overwriteComplex(to, from);
    }

  private:
    /// The xloper type made safe for use in switch statements by zeroing
    /// the memory control flags blanked.
    int ExcelObj::xtype() const
    {
      return xltype & ~(msxll::xlbitXLFree | msxll::xlbitDLLFree);
    }

    static void overwriteComplex(ExcelObj& to, const ExcelObj& from);
  };
}