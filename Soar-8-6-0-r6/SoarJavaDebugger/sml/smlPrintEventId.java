/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 1.3.24
 *
 * Do not make changes to this file unless you know what you are doing--modify
 * the SWIG interface file instead.
 * ----------------------------------------------------------------------------- */

package sml;

public final class smlPrintEventId {
  public final static smlPrintEventId smlEVENT_LOG_ERROR = new smlPrintEventId("smlEVENT_LOG_ERROR", smlWorkingMemoryEventId.smlEVENT_OUTPUT_PHASE_CALLBACK.swigValue() + 1);
  public final static smlPrintEventId smlEVENT_LOG_WARNING = new smlPrintEventId("smlEVENT_LOG_WARNING");
  public final static smlPrintEventId smlEVENT_LOG_INFO = new smlPrintEventId("smlEVENT_LOG_INFO");
  public final static smlPrintEventId smlEVENT_LOG_DEBUG = new smlPrintEventId("smlEVENT_LOG_DEBUG");
  public final static smlPrintEventId smlEVENT_STRUCTURED_OUTPUT = new smlPrintEventId("smlEVENT_STRUCTURED_OUTPUT");
  public final static smlPrintEventId smlEVENT_PRINT = new smlPrintEventId("smlEVENT_PRINT");

  public final int swigValue() {
    return swigValue;
  }

  public String toString() {
    return swigName;
  }

  public static smlPrintEventId swigToEnum(int swigValue) {
    if (swigValue < swigValues.length && swigValues[swigValue].swigValue == swigValue)
      return swigValues[swigValue];
    for (int i = 0; i < swigValues.length; i++)
      if (swigValues[i].swigValue == swigValue)
        return swigValues[i];
    throw new IllegalArgumentException("No enum " + smlPrintEventId.class + " with value " + swigValue);
  }

  private smlPrintEventId(String swigName) {
    this.swigName = swigName;
    this.swigValue = swigNext++;
  }

  private smlPrintEventId(String swigName, int swigValue) {
    this.swigName = swigName;
    this.swigValue = swigValue;
    swigNext = swigValue+1;
  }

  private static smlPrintEventId[] swigValues = { smlEVENT_LOG_ERROR, smlEVENT_LOG_WARNING, smlEVENT_LOG_INFO, smlEVENT_LOG_DEBUG, smlEVENT_STRUCTURED_OUTPUT, smlEVENT_PRINT };
  private static int swigNext = 0;
  private final int swigValue;
  private final String swigName;
}


