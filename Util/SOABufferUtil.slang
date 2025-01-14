public interface ISOAUnitTrait
{
    associatedtype Word : __BuiltinArithmeticType;
    static const uint kWordsPerUnit;
};

public struct SOAUnitTrait<Word_T : __BuiltinArithmeticType, let WordsPerUnit_V : uint> : ISOAUnitTrait
{
    public typealias Word = Word_T;
    public static const uint kWordsPerUnit = WordsPerUnit_V;
};

public interface ISOATrait : ISOAUnitTrait
{
    static const uint kWordsPerElem;
    static const uint kUnitsPerElem;
    static const uint kWordsPerExt;
    static const uint kPaddedWordsPerExt;
    associatedtype Unit;
    associatedtype Ext;
    associatedtype PaddedExt;
    public static PaddedExt padExt(Ext);
    public static Ext unpadExt(PaddedExt);
};

public struct SOATrait<UnitTrait_T : ISOAUnitTrait, let WordsPerElem_V : uint> : ISOATrait
{
    public typealias Word = UnitTrait_T::Word;
    public static const uint kWordsPerUnit = UnitTrait_T::kWordsPerUnit;
    public static const uint kWordsPerElem = WordsPerElem_V;
    public static const uint kUnitsPerElem =
        (WordsPerElem_V % kWordsPerUnit == 0) ? (WordsPerElem_V / kWordsPerUnit - 1) : (WordsPerElem_V / kWordsPerUnit);
    public static const uint kWordsPerExt = (WordsPerElem_V % kWordsPerUnit == 0) ? kWordsPerUnit : (WordsPerElem_V % kWordsPerUnit);
    public static const uint kPaddedWordsPerExt = kWordsPerExt == 3 ? 4 : kWordsPerExt;
    public typealias Unit = vector<Word, kWordsPerUnit>;
    public typealias Ext = vector<Word, kWordsPerExt>;
    public typealias PaddedExt = vector<Word, kPaddedWordsPerExt>;
    public static PaddedExt padExt(Ext ext)
    {
        PaddedExt paddedExt;
        [ForceUnroll]
        for (uint i = 0; i < kWordsPerExt; ++i)
            paddedExt[i] = ext[i];
        [ForceUnroll]
        for (uint i = kWordsPerExt; i < kPaddedWordsPerExt; ++i)
            paddedExt[i] = Word(0);
        return paddedExt;
    }
    public static Ext unpadExt(PaddedExt paddedExt)
    {
        Ext ext;
        [ForceUnroll]
        for (uint i = 0; i < kWordsPerExt; ++i)
            ext[i] = paddedExt[i];
        return ext;
    }
};

public interface ISOAElement<Trait_T : ISOATrait>
{
    SOATuple<Trait_T> toSOA();
    static This fromSOA(SOATuple<Trait_T>);
};

public struct SOATuple<Trait_T : ISOATrait> : ISOAElement<Trait_T>
{
    public static const uint kWordsPerUnit = Trait_T::kWordsPerUnit;
    public static const uint kWordsPerElem = Trait_T::kWordsPerElem;
    public static const uint kUnitsPerElem = Trait_T::kUnitsPerElem;
    public static const uint kWordsPerExt = Trait_T::kWordsPerExt;
    public static const uint kPaddedWordsPerExt = Trait_T::kPaddedWordsPerExt;
    public typealias Word = Trait_T::Word;
    public typealias Unit = Trait_T::Unit;
    public typealias Ext = Trait_T::Ext;
    public typealias PaddedExt = Trait_T::PaddedExt;
    public typealias Trait = Trait_T;

    public Trait_T::Unit units[Trait_T::kUnitsPerElem];
    public Trait_T::Ext ext;

    public SOATuple<Trait_T> toSOA() { return this; }
    public static SOATuple<Trait_T> fromSOA(SOATuple<Trait_T> soa) { return soa; }
};

public struct SOABuffer<Trait_T : ISOATrait>
{
    public static const uint kWordsPerUnit = Trait_T::kWordsPerUnit;
    public static const uint kWordsPerElem = Trait_T::kWordsPerElem;
    public static const uint kUnitsPerElem = Trait_T::kUnitsPerElem;
    public static const uint kWordsPerExt = Trait_T::kWordsPerExt;
    public static const uint kPaddedWordsPerExt = Trait_T::kPaddedWordsPerExt;
    public typealias Word = Trait_T::Word;
    public typealias Unit = Trait_T::Unit;
    public typealias Ext = Trait_T::Ext;
    public typealias PaddedExt = Trait_T::PaddedExt;
    public typealias Trait = Trait_T;

    public StructuredBuffer<Trait_T::Unit> unitBufs[Trait_T::kUnitsPerElem];
    public StructuredBuffer<Trait_T::PaddedExt> extBuf;

    public T load<T : ISOAElement<Trait_T>>(uint id)
    {
        SOATuple<Trait_T> a;
        [ForceUnroll]
        for (uint i = 0; i < Trait_T::kUnitsPerElem; ++i)
            a.units[i] = unitBufs[i][id];
        a.ext = Trait_T::unpadExt(extBuf[id]);
        return T::fromSOA(a);
    }
};

public struct RWSOABuffer<Trait_T : ISOATrait>
{
    public static const uint kWordsPerUnit = Trait_T::kWordsPerUnit;
    public static const uint kWordsPerElem = Trait_T::kWordsPerElem;
    public static const uint kUnitsPerElem = Trait_T::kUnitsPerElem;
    public static const uint kWordsPerExt = Trait_T::kWordsPerExt;
    public static const uint kPaddedWordsPerExt = Trait_T::kPaddedWordsPerExt;
    public typealias Word = Trait_T::Word;
    public typealias Unit = Trait_T::Unit;
    public typealias Ext = Trait_T::Ext;
    public typealias PaddedExt = Trait_T::PaddedExt;
    public typealias Trait = Trait_T;

    public RWStructuredBuffer<Trait_T::Unit> unitBufs[Trait_T::kUnitsPerElem];
    public RWStructuredBuffer<Trait_T::PaddedExt> extBuf;

    public T load<T : ISOAElement<Trait_T>>(uint id)
    {
        SOATuple<Trait_T> a;
        [ForceUnroll]
        for (uint i = 0; i < Trait_T::kUnitsPerElem; ++i)
            a.units[i] = unitBufs[i][id];
        a.ext = Trait_T::unpadExt(extBuf[id]);
        return T::fromSOA(a);
    }

    public void store<T : ISOAElement<Trait_T>>(uint id, T elem)
    {
        SOATuple<Trait_T> a = elem.toSOA();
        [ForceUnroll]
        for (uint i = 0; i < Trait_T::kUnitsPerElem; ++i)
            unitBufs[i][id] = a.units[i];
        extBuf[id] = Trait_T::padExt(a.ext);
    }
};

/* public extension<Word_T : __BuiltinArithmeticType, let WordsPerUnit_V : uint, let WordsPerElem_V : uint>
    SOATuple<SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>>
{
    public static This add(This l, This r)
    {
        This t;
        [ForceUnroll]
        for (uint i = 0; i < Trait::kUnitsPerElem; ++i)
            t.units[i] = l.units[i] + r.units[i];
        t.ext = l.ext + r.ext;
        return t;
    }
    public static This sub(This l, This r)
    {
        This t;
        [ForceUnroll]
        for (uint i = 0; i < Trait::kUnitsPerElem; ++i)
            t.units[i] = l.units[i] - r.units[i];
        t.ext = l.ext - r.ext;
        return t;
    }
    public static This mul(This l, This r)
    {
        This t;
        [ForceUnroll]
        for (uint i = 0; i < Trait::kUnitsPerElem; ++i)
            t.units[i] = l.units[i] * r.units[i];
        t.ext = l.ext * r.ext;
        return t;
    }
    public static This mul(Word_T l, This r)
    {
        This t;
        [ForceUnroll]
        for (uint i = 0; i < Trait::kUnitsPerElem; ++i)
            t.units[i] = l * r.units[i];
        t.ext = l * r.ext;
        return t;
    }
} */
