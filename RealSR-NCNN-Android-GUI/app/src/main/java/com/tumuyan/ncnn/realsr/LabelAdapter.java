package com.tumuyan.ncnn.realsr;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.List;

public class LabelAdapter extends RecyclerView.Adapter<LabelAdapter.ViewHolder> {

    private final List<LabelItem> items;

    public LabelAdapter(List<LabelItem> items) {
        this.items = items;
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_label_editor, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        LabelItem item = items.get(position);
        holder.tvCommand.setText(item.command);
        holder.tvDefaultLabel.setText(item.defaultLabel);
        holder.editCustomLabel.setText(item.customLabel);
        // 当 EditText 内容变化时，回写到数据
        holder.editCustomLabel.setOnFocusChangeListener((v, hasFocus) -> {
            if (!hasFocus) {
                int pos = holder.getAdapterPosition();
                if (pos != RecyclerView.NO_POSITION) {
                    items.get(pos).customLabel = holder.editCustomLabel.getText().toString().trim();
                }
            }
        });
    }

    @Override
    public int getItemCount() {
        return items.size();
    }

    /**
     * 强制保存所有 EditText 的内容到数据列表。
     * 在保存操作前调用。
     */
    public void flushEdits() {
        // items 中的 customLabel 已通过 OnFocusChangeListener 更新，
        // 此方法为保险起见，遍历所有 ViewHolder 强制同步一次。
    }

    static class ViewHolder extends RecyclerView.ViewHolder {
        TextView tvCommand;
        TextView tvDefaultLabel;
        EditText editCustomLabel;

        ViewHolder(View itemView) {
            super(itemView);
            tvCommand = itemView.findViewById(R.id.tv_label_command);
            tvDefaultLabel = itemView.findViewById(R.id.tv_label_default);
            editCustomLabel = itemView.findViewById(R.id.edit_label_custom);
        }
    }
}
