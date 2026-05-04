from .input_interaction import validate_input_and_interaction
from .ordering import validate_log_order
from .parsing import read_log_lines
from .required_markers import validate_required_markers
from .streaming_rendering import validate_streaming_and_rendering


def validate(log_file):
    if not log_file.exists():
        return [f"{log_file}: missing client app launch probe log"]

    lines = read_log_lines(log_file)
    if not lines or not lines[0].startswith("crash_marker=/tmp/octaryn-crash-"):
        return [f"{log_file}: missing crash diagnostics marker line, actual {lines}"]

    errors = []
    validate_required_markers(log_file, lines, errors)
    validate_streaming_and_rendering(log_file, lines, errors)
    validate_input_and_interaction(log_file, lines, errors)
    validate_log_order(log_file, lines, errors)
    return errors
