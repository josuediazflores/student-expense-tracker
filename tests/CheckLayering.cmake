# Author: Huy Tran
#
# Layering check for the MVC split. Run as:
#   cmake -DSOURCE_DIR=<repo> -P tests/CheckLayering.cmake
#
# It reads the #include lines of each source file and fails if a layer depends on
# something it must not know about:
#   Model      knows nothing about the Controller, View, display interface, GUI
#              classes, or Qt signals.
#   Controller does not know the View or GUI classes, and its header does not
#              include Model headers.
#   View       does not see the Model headers.
#   Display    interface header includes no project header.
#   Store      (BudgetStore) knows nothing about the Controller, View, display interface,
#              GUI classes, or Qt signals. The Model must not know about storage, the View
#              only calls the Controller, and the Controller header forward-declares the
#              store, so none of those include BudgetStore.h.
#   restoreBudget  is for the store's load only: no src file other than BudgetModel.h,
#              BudgetModel.cpp and BudgetStore.cpp may contain the text "restoreBudget(",
#              so the duplicate-budget rule cannot be bypassed from the Controller or View.
# Only direct includes are inspected. That is enough: the Controller header may not
# include Model headers, so nothing reaches the View through it.

if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "CheckLayering.cmake needs -DSOURCE_DIR=<repo root>")
endif()

# Include names that are Qt GUI/Widgets modules or GUI classes.
set(GUI_PATTERNS
    "^Qt(Widgets|Gui)(/|$)"
    "^Q[A-Za-z]*Widget"
    "^(QApplication|QGuiApplication|QLabel|QPushButton|QLineEdit|QDateEdit|QComboBox|QSpinBox|QDoubleSpinBox|QCheckBox|QRadioButton|QFormLayout|QBoxLayout|QVBoxLayout|QHBoxLayout|QGridLayout|QHeaderView|QAbstractItemView|QTableView|QListView|QTreeView|QMessageBox|QMainWindow|QDialog|QFont|QColor|QPalette|QPixmap|QImage|QIcon|QPainter|QStyle|QAction|QMenu)$")

# Whole-line patterns for Qt signal machinery.
set(SIGNALS_PATTERN "(^|[^A-Za-z0-9_])(Q_OBJECT|Q_SIGNALS|Q_EMIT|emit|signals)([^A-Za-z0-9_]|$)")
set(OBJECT_PATTERN "(^|[^A-Za-z0-9_])Q_OBJECT([^A-Za-z0-9_]|$)")

set(MODEL_PROJECT_HEADERS "(^|/)Budget(Controller|View|Display)[.]h$")
set(STORE_HEADER "(^|/)BudgetStore[.]h$")
set(VIEW_HEADER "(^|/)BudgetView[.]h$")
set(MODEL_HEADERS "(^|/)(BudgetModel|DecimalCents)[.]h$")

set(violations 0)

# check_file(<relpath> <layer> [OPTIONAL])
#   layer: MODEL | CONTROLLER_HEADER | CONTROLLER_SOURCE | VIEW | DISPLAY | STORE
#   OPTIONAL: a missing file is skipped instead of counted (the build fails without it).
function(check_file relpath layer)
    set(path "${SOURCE_DIR}/${relpath}")
    set(found 0)

    if(NOT EXISTS "${path}")
        if("OPTIONAL" IN_LIST ARGN)
            return()
        endif()
        message("LAYERING CHECK: missing expected file ${relpath}")
        math(EXPR total "${violations} + 1")
        set(violations ${total} PARENT_SCOPE)
        return()
    endif()

    # Include lines only: code lines holding ';' or '[' never become list items.
    file(STRINGS "${path}" include_lines REGEX "^[ \t]*#[ \t]*include")
    foreach(line IN LISTS include_lines)
        if(NOT line MATCHES "^[ \t]*#[ \t]*include[ \t]*([<\"])([^>\"]+)[>\"]")
            continue()
        endif()
        set(delimiter "${CMAKE_MATCH_1}")
        set(name "${CMAKE_MATCH_2}")

        set(reasons "")
        set(is_gui FALSE)
        foreach(gui IN LISTS GUI_PATTERNS)
            if(name MATCHES "${gui}")
                set(is_gui TRUE)
            endif()
        endforeach()

        if(layer STREQUAL "MODEL")
            if(name MATCHES "${MODEL_PROJECT_HEADERS}")
                list(APPEND reasons "the Model must not know the Controller, View or Display")
            endif()
            if(name MATCHES "${STORE_HEADER}")
                list(APPEND reasons "the Model must not know about storage")
            endif()
            if(is_gui)
                list(APPEND reasons "the Model must not use GUI classes")
            endif()
        elseif(layer STREQUAL "CONTROLLER_HEADER" OR layer STREQUAL "CONTROLLER_SOURCE")
            if(name MATCHES "${VIEW_HEADER}")
                list(APPEND reasons "the Controller must not know the View")
            endif()
            if(is_gui)
                list(APPEND reasons "the Controller must not use GUI classes")
            endif()
            if(layer STREQUAL "CONTROLLER_HEADER" AND name MATCHES "${MODEL_HEADERS}")
                list(APPEND reasons "the Controller header must not expose Model headers")
            endif()
            if(layer STREQUAL "CONTROLLER_HEADER" AND name MATCHES "${STORE_HEADER}")
                list(APPEND reasons "the Controller header must forward-declare the store, not include it")
            endif()
        elseif(layer STREQUAL "VIEW")
            if(name MATCHES "${MODEL_HEADERS}")
                list(APPEND reasons "the View must not see Model headers")
            endif()
            if(name MATCHES "${STORE_HEADER}")
                list(APPEND reasons "the View only calls the Controller, never the store")
            endif()
        elseif(layer STREQUAL "STORE")
            if(name MATCHES "${MODEL_PROJECT_HEADERS}")
                list(APPEND reasons "the store must not know the Controller, View or Display")
            endif()
            if(is_gui)
                list(APPEND reasons "the store must not use GUI classes")
            endif()
        elseif(layer STREQUAL "DISPLAY")
            get_filename_component(base "${name}" NAME)
            if(delimiter STREQUAL "\"" OR EXISTS "${SOURCE_DIR}/src/${base}")
                list(APPEND reasons "the display interface must not include project headers")
            endif()
        endif()

        foreach(reason IN LISTS reasons)
            message("LAYERING VIOLATION: ${relpath} includes ${name} -- ${reason}")
            math(EXPR found "${found} + 1")
        endforeach()
    endforeach()

    # Signals / Q_OBJECT: scan the file text line by line, without making it a list.
    set(content_pattern "")
    if(layer STREQUAL "MODEL" OR layer STREQUAL "STORE")
        set(content_pattern "${SIGNALS_PATTERN}")
    elseif(layer STREQUAL "CONTROLLER_HEADER" OR layer STREQUAL "CONTROLLER_SOURCE")
        set(content_pattern "${OBJECT_PATTERN}")
    endif()
    if(NOT content_pattern STREQUAL "")
        file(READ "${path}" text)
        string(REPLACE "\r" "" text "${text}")
        while(NOT text STREQUAL "")
            string(FIND "${text}" "\n" newline)
            if(newline EQUAL -1)
                set(current "${text}")
                set(text "")
            else()
                string(SUBSTRING "${text}" 0 ${newline} current)
                math(EXPR rest_start "${newline} + 1")
                string(SUBSTRING "${text}" ${rest_start} -1 text)
            endif()
            if(current MATCHES "${content_pattern}")
                string(STRIP "${current}" shown)
                message("LAYERING VIOLATION: ${relpath} uses Qt signals/Q_OBJECT -- ${shown}")
                math(EXPR found "${found} + 1")
            endif()
        endwhile()
    endif()

    math(EXPR total "${violations} + ${found}")
    set(violations ${total} PARENT_SCOPE)
endfunction()

check_file("src/BudgetModel.h" MODEL)
check_file("src/BudgetModel.cpp" MODEL)
check_file("src/DecimalCents.h" MODEL)
check_file("src/DecimalCents.cpp" MODEL)
check_file("src/BudgetController.h" CONTROLLER_HEADER)
check_file("src/BudgetController.cpp" CONTROLLER_SOURCE)
check_file("src/BudgetView.h" VIEW)
check_file("src/BudgetView.cpp" VIEW)
check_file("src/BudgetDisplay.h" DISPLAY)
check_file("src/BudgetStore.h" STORE)
# The .cpp is optional here: without it the build fails anyway, and the missing header
# is the single violation that marks the store as not yet written.
check_file("src/BudgetStore.cpp" STORE OPTIONAL)

# restoreBudget skips the duplicate-budget rule, so only the Model that defines it and the
# store's load that needs it may name it. Every file under src/ is scanned, main.cpp too.
file(GLOB src_files RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/src/*")
foreach(relpath IN LISTS src_files)
    if(relpath MATCHES "^src/(BudgetModel[.]h|BudgetModel[.]cpp|BudgetStore[.]cpp)$"
       OR IS_DIRECTORY "${SOURCE_DIR}/${relpath}")
        continue()
    endif()
    file(READ "${SOURCE_DIR}/${relpath}" restore_text)
    string(FIND "${restore_text}" "restoreBudget(" restore_position)
    if(NOT restore_position EQUAL -1)
        message("LAYERING VIOLATION: ${relpath} contains restoreBudget( -- only the Model and the store's load may use it")
        math(EXPR violations "${violations} + 1")
    endif()
endforeach()

if(violations GREATER 0)
    message(FATAL_ERROR "Layering check failed (${violations} violations).")
endif()
message("Layering check passed.")
