
# TODO: how can we synchronise the help here with what you see when you actually import xloil_core

def in_wizard():
    """ 
    Returns true if the function is being invoked from the function 
    wizard. Costly functions should exit in this case. But checking 
    for the wizard is itself not cheap, so use this sparingly.
    """
    pass

def log(msg, level="info"):
    pass

class Range:
    """
    Similar to an Excel Range object, this class allows access to an area on a 
    worksheet. It uses similar syntax to Excel's object, supporting the ``cell``
    and ``range`` functions.
    """
    def range(self, from_row, from_col, num_rows=-1, num_cols=-1, to_row=None, to_col=None):
        """ 
        Creates a subrange starting from the specified row and columns and ending
        either at a specified row and column or spannig a number of rows and columns.
        Using negative numbers or num_rows or num_cols means an offset from the end,
        as per the usual python array conventions.
        """
        pass
    def cell(self, row, col):
        """ Returns a Range object which consists of the single cell specified """
        pass
    @property
    def value(self):
        """ 
        Property which gets or sets the value for a range. A fetched value is converted
        to the most appropriate Python type using the normal generic converter.

        If you use a horizontal array for the assignemnt, it is duplicated down to fill 
        the entire rectangle. If you use a vertical array, it is duplicated right to fill 
        the entire rectangle. If you use a rectangular array, and it is too small for the 
        rectangular range you want to put it in, that range is padded with #N/As.
        """
        pass
    def set(self, val):
        """
        Sets the data in the range to the provided value. If a single value is passed
        all cells will be set to the value. If a 2d-array is provided, the array will be
        pasted at the top-left of the range with the remainging cells being set to #N/A.
        If a 1d array is provided it will be pasted at the top left and repeated down or
        right depending on orientation.
        """
        pass
    def clear(self):
        """
        Sets all values in the range to the Nil/Empty type
        """
        pass
    def address(self,local=False):
        """
        Gets the range address in A1 format e.g.
            local=False # [Book1]Sheet1!F37
            local=True  # F37
        """
        pass
    @property
    def nrows(self):
        """ Returns the number of rows in the range """
        pass
    @property
    def ncols(self):
        """ Returns the number of columns in the range """
        pass
    def __getitem__(self, tuple):
        """ 
        Given a 2-tuple, slices the range to return a sub Range or a 
        single element.
        """
        pass

class ExcelArray:
    """
    A view of a internal Excel array which can be manipulated without
    copying the underlying data. It's not a general purpose array class 
    but rather used to create efficiencies in type converters.
    
    It can be accessed and sliced using the usual syntax:
        x[1, 1] # The value at 1,1 as int, str, float, etc.
        x[1, :] # The second row as another ExcelArray
    
    """
    def __getitem__(self, tuple):
        """ 
        Given a 2-tuple, slices the array to return a sub ExcelArray or a 
        single element.
        """
        pass
    def to_numpy(self, dtype=None, dims=2):
        """
        Converts the array to a numpy array. If dtype is None, attempts to 
        discover one, otherwise raises an exception if values cannot be 
        converted to the specified dtype. dims can be 1 or 2
        """
        pass
    @property
    def dims(self):
        """ 
        Property which gives the dimension of the array: 1 or 2
        """
        pass
    @property
    def nrows(self):
        """ Returns the number of rows in the array """
        pass
    @property
    def ncols(self):
        """ Returns the number of columns in the array """
        pass

class CellError:
    """
    Enum-type class which represents an Excel error condition of the 
    form #N/A!, #NAME!, etc passed as a function argument. If your
    function does not use a specific type-converter it may be passed 
    an object of this type, which it can handle based on error condition.
    """
    Null = None
    Div0 = None
    Value = None
    Ref = None
    Name = None
    Num = None
    NA = None
    GettingData = None

class _CustomConverter:
    """
    This is the interface class for custom type converters to allow them
    to be called from the Core.
    """
    def __init__(self, callable):
        pass

class _Event:
    def __iadd__(self, handler):
        """
        Registers an event handler function, for example:
            
            event.NewWorkbook += lambda wb_name: print(wb_name)
            
        """
        pass
    def __isub__(self, handler):
        """
        Removes a previously registered event handler function
        """
        pass
    def handlers(self):
        """
        Returns a list of registered handlers for this event
        """
        pass

# Strictly speaking, xloil_core.event is a module but this
# should give the right doc strings
class Event:
    """
    Contains hooks for events driven by user interaction with Excel. The
    events correspond to COM/VBA events and are described in detail at
    `Excel.Appliction <https://docs.microsoft.com/en-us/office/vba/api/excel.application(object)#events>`_


    Notes:
        * The `CalcCancelled` and `WorkbookAfterClose` event are not part of the 
            Application object, see their individual documentation.
        * Where an event has reference parameter, for example the `cancel` bool in
            `WorkbookBeforeSave`, you need to set the value using `cancel.value=True`.
            This is because python does not support reference parameters for primitive types. 

    Examples
    --------

    ::

        def greet(workbook, worksheet):
            xlo.Range(f"[{workbook}]{worksheet}!A1") = "Hello!"

        xlo.event.WorkbookNewSheet += greet

    """

    AfterCalculate= _Event()
    """
    Called when the user interrupts calculation by interacting with Excel.
    """
    CalcCancelled= _Event()
    NewWorkbook= _Event()
    SheetSelectionChange= _Event()
    SheetBeforeDoubleClick= _Event()
    SheetBeforeRightClick= _Event()
    SheetActivate= _Event()
    SheetDeactivate= _Event()
    SheetCalculate= _Event()
    SheetChange= _Event()
    WorkbookOpen= _Event()
    WorkbookActivate= _Event()
    WorkbookDeactivate= _Event()
    """
    Excel's event *WorkbookBeforeClose*, is  cancellable by the user so it is not 
    possible to know if the workbook actually closed.  When xlOil calls 
    `WorkbookAfterClose`, the workbook is certainly closed, but it may be some time
    since that closure happened.

    The event is not called for each workbook when xlOil exits.
    """
    WorkbookAfterClose= _Event()
    WorkbookBeforeSave= _Event()
    WorkbookBeforePrint= _Event()
    WorkbookNewSheet= _Event()
    WorkbookAddinInstall= _Event()
    WorkbookAddinUninstall= _Event()

event = Event()

class Cache:
    """
    Provides a link to the Python object cache

    Examples
    --------

    ::
        
        @xlo.func
        def myfunc(x):
            return xlo.cache(MyObject(x)) # <- equivalent to .add(...)

        @xlo.func
        def myfunc2(array: xlo.Array(str), i):
            return xlo.cache[array[i]] # <- equivalent to .get(...)

    """

    def add(self, obj):
        """
        Adds an object to the cache and returns a reference string
        based on the currently calculating cell.

        xlOil automatically adds unconvertible returned objects to the cache,
        so this function is useful to force a recognised object, such as an 
        iterable into the cache, or to return a list of cached objects.
        """
        pass

    def get(self, ref:str):
        """
        Fetches an object from the cache given a reference string.
        Returns None if not found
        """
        pass

    def contains(self, ref:str):
        """
        Returns True if the given reference string links to a valid object
        """
        pass

    __contains__ = contains
    __getitem__ = get
    __call__ = add

cache = Cache()

class RtdPublisher:
    """
    RTD servers use a publisher/subscriber model with the 'topic' as the key
    The publisher class is linked to a single topic string.

    Typically the publisher will do nothing on construction, but when it detects
    a subscriber using the connect() method, it creates a background publishing task
    When disconnect() indicates there are no subscribers, it cancels this task with
    a call to stop()

    If the task is slow to return or spin up, it could be started the constructor  
    and kept it running permanently, regardless of subscribers.

    The publisher should call RtdServer.publish() to push values to subscribers.
    """

    def __init__(self):
        """
        This __init__ method must be called explicitly by subclasses or 
        pybind will fatally crash Excel.
        """
        pass
    def connect(self, num_subscribers):
        """
        Called by the RtdServer when a sheet function subscribes to this 
        topic. Typically a topic will start up its publisher on the first
        subscriber, i.e. when num_subscribers == 1
        """
        pass
    def disconnect(self, num_subscribers):
        """
        Called by the RtdServer when a sheet function disconnects from this 
        topic. This happens when the function arguments are changed the
        function deleted. Typically a topic will shutdown its publisher 
        when num_subscribers == 0.

        Whilst the topic remains live, it may still receive new connection
        requests, so generally avoid finalising in this method.
        """
        pass
    def stop(self):
        """
        Called by the RtdServer to indicate that a topic should shutdown
        and dependent threads or tasks and finalise resource usage
        """
        pass
    def done(self) -> bool:
        """
        Returns True if the topic can safely be deleted without 
        leaking resources.
        """
        pass
    def topic(self) -> str:
        """
        Returns the name of the topic
        """
        pass

class RtdServer:
    """
    An RtdServer sits above an Rtd COM server. Each new RtdServer creates a
    new underlying COM server. The manager connects publishers and subscribers
    for topics, identified by a string. 

    A topic publisher is registered using start(). Subsequent calls to subscribe()
    will connect this topic and tell Excel that the current calling cell should be
    recalculated when a new value is published.

    RTD sits outside of Excel's normal calc cycle: publishers can publish new values 
    at any time, triggering a re-calc of any cells containing subscribers. Note the
    re-calc will only happen 'live' if Excel's caclulation mode is set to automatic
    """

    def start(self, topic:RtdPublisher):
        """
        Registers an RtdPublisher publisher with this manager. The RtdPublisher receives
        notification when the number of subscribers changes
        """
        pass
    def publish(self, topic:str, value):
        """
        Publishes a new value for the specified topic and updates all subscribers.
        This function can be called even if no RtdPublisher has been started.

        This function does not use any Excel API and is safe to call at any time
        on any thread.
        """
        pass
    def subscribe(self, topic:str):
        """
        Subscribes to the specified topic. If no publisher for the topic currently 
        exists, it returns None, but the subscription is held open and will connect
        to a publisher created later. If there is no published value, it will return 
        CellError.NA.  
        
        This calls Excel's RTD function, which means the calling cell will be
        recalculated every time a new value is published.

        Calling this function outside of a worksheet function called by Excel may
        produce undesired results and possibly crash Excel.
        """
        pass
    def peek(self, topic:str, converter=None):
        """
        Looks up a value for a specified topic, but does not subscribe.
        If there is no active publisher for the topic, it returns None.
        If there is no published value, it will return CellError.NA.

        This function does not use any Excel API and is safe to call at
        any time on any thread.
        """
        pass

def register_functions(module, function_holders):
    pass

def deregister_functions(module, function_names):
    pass

def get_event_loop():
        """
        Returns the asyncio event loop assoicated with the async background
        worker thread.
        """
        pass