require 'test/unit'
require 'date'

begin
  require 'calendar'
  include  Calendar
rescue LoadError
end

class TestDateBase < Test::Unit::TestCase

  def test__inf
    assert_equal(0, Date::Infinity.new(-1) <=> Date::Infinity.new(-1))
    assert_equal(-1, Date::Infinity.new(-1) <=> Date::Infinity.new(+1))
    assert_equal(-1, Date::Infinity.new(-1) <=> 0)

    assert_equal(1, Date::Infinity.new(+1) <=> Date::Infinity.new(-1))
    assert_equal(0, Date::Infinity.new(+1) <=> Date::Infinity.new(+1))
    assert_equal(1, Date::Infinity.new(+1) <=> 0)

    assert_equal(1, 0 <=> Date::Infinity.new(-1))
    assert_equal(-1, 0 <=> Date::Infinity.new(+1))
    assert_equal(0, 0 <=> 0)

    assert_equal(0, Date::ITALY <=> Date::ITALY)
    assert_equal(-1, Date::ITALY <=> Date::ENGLAND)
    assert_equal(-1, Date::ITALY <=> Date::JULIAN)
    assert_equal(1, Date::ITALY <=> Date::GREGORIAN)

    assert_equal(1, Date::ENGLAND <=> Date::ITALY)
    assert_equal(0, Date::ENGLAND <=> Date::ENGLAND)
    assert_equal(-1, Date::ENGLAND <=> Date::JULIAN)
    assert_equal(1, Date::ENGLAND <=> Date::GREGORIAN)

    assert_equal(1, Date::JULIAN <=> Date::ITALY)
    assert_equal(1, Date::JULIAN <=> Date::ENGLAND)
    assert_equal(0, Date::JULIAN <=> Date::JULIAN)
    assert_equal(1, Date::JULIAN <=> Date::GREGORIAN)

    assert_equal(-1, Date::GREGORIAN <=> Date::ITALY)
    assert_equal(-1, Date::GREGORIAN <=> Date::ENGLAND)
    assert_equal(-1, Date::GREGORIAN <=> Date::JULIAN)
    assert_equal(0, Date::GREGORIAN <=> Date::GREGORIAN)
  end

  def test_jd
    assert_equal(1 << 33, Date.jd(1 << 33).jd)
  end

  def test_leap?
    assert_equal(true, Date.julian_leap?(1900))
    assert_equal(false, Date.julian_leap?(1999))
    assert_equal(true, Date.julian_leap?(2000))

    assert_equal(false, Date.gregorian_leap?(1900))
    assert_equal(false, Date.gregorian_leap?(1999))
    assert_equal(true, Date.gregorian_leap?(2000))

    assert_equal(Date.leap?(1990), Date.gregorian_leap?(1900))
    assert_equal(Date.leap?(1999), Date.gregorian_leap?(1999))
    assert_equal(Date.leap?(2000), Date.gregorian_leap?(2000))
  end

end
